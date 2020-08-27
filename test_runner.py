import distutils.dir_util
import os
import platform
import time
import unittest
from datetime import datetime
from subprocess import check_output, CalledProcessError, STDOUT
from typing import Iterable, Optional, List
from unittest import TestCase

from teamcity import is_running_under_teamcity
from teamcity.unittestpy import TeamcityTestRunner

JAVA_HOME = os.getenv("JAVA_HOME")
AGENT_NAME = "memory_agent"
IS_UNDER_TEAMCITY = is_running_under_teamcity()
PROXY_COMPILED_PATH = os.path.join('test_data', 'proxy', 'build') if IS_UNDER_TEAMCITY else None

if JAVA_HOME is None:
    print("Java not found. Please specify JAVA_HOME and try again.")
    exit(1)


def get_java_executable() -> str:
    return os.path.join(JAVA_HOME, 'bin', 'java')


def get_java_compiler() -> str:
    return os.path.join(JAVA_HOME, 'bin', 'javac')


def get_java_architecture() -> str:
    out = check_output([get_java_executable(), '-version'], stderr=STDOUT).decode('utf-8')
    if out.find('64-Bit'):
        return '64bit'
    return '32bit'


def output_file(name: str, directory: Optional[str] = None) -> str:
    result = '{}.out'.format(name)
    if directory is not None:
        result = os.path.join(directory, result)
    return result


def dynamic_library_name(lib_name) -> str:
    def dynamic_lib_format() -> str:
        os_type = platform.system()
        if os_type == "Windows":
            architecture = get_java_architecture()
            if architecture == "32bit":
                return '{}32.dll'
            else:
                return '{}.dll'
        if os_type == "Darwin":
            return 'lib{}.dylib'
        if os_type == "Linux":
            return 'lib{}.so'
        raise Exception("Unknown OS type")

    return dynamic_lib_format().format(lib_name)


def agent_lib_name() -> str:
    return dynamic_library_name(AGENT_NAME)


def find_agent_in_project_directory(file_name: str):
    result = list()
    for root, dirs, files in os.walk('.'):
        result.extend((os.path.join(root, file) for file in files if file_name == file))
    if len(result) == 0:
        raise AssertionError("Agent not found")
    if len(result) > 1:
        raise AssertionError("Too many agents found: " + str(result))
    return result[0]


def find_agent_file() -> str:
    library_file_name = agent_lib_name()
    if IS_UNDER_TEAMCITY:
        return os.path.join('bin', library_file_name)
    else:
        return find_agent_in_project_directory(library_file_name)


class JavaCompiler:
    def __init__(self, javac: str, output_path: str) -> None:
        self.__output = output_path
        self.__javac = javac

    def compile_java(self, source_files: List[str], classpath: str):
        args = list()
        args.append(self.__javac)
        args.extend(['-d', self.__output])
        if classpath is not None:
            args.extend(['-classpath', classpath])
        args.extend(source_files)
        check_output(args)


class Test:
    def __init__(self, name: str, output: str, src_dir: str) -> None:
        self.__name = name
        self.__output = output
        self.__path = os.path.join(src_dir, name)

    def name(self) -> str:
        return self.__name

    def expected_output(self) -> Optional[str]:
        return self.__output

    def src_path(self):
        return self.__path


class TestResult:
    def __init__(self, test: Test, output: str) -> None:
        self.__output = output
        self.__test = test

    def get_output(self) -> str:
        return self.__output

    def get_test(self) -> Test:
        return self.__test


class TestRunner:
    def __init__(self, java, build_dir: str, output_dir: str, agent_path: str) -> None:
        self.__java = java
        self.__build_dir = build_dir
        self.__output_directory = output_dir
        self.__agent_path = os.path.abspath(agent_path)

    def run(self, test: Test) -> TestResult:
        if not os.path.exists(self.__output_directory):
            os.makedirs(self.__output_directory)

        args = list()
        args.append(self.__java)
        args.append('-agentpath:{}=3'.format(self.__agent_path))
        args.extend(['-classpath', self.__build_dir])
        args.append(test.name())

        out = check_output(args).decode("utf-8").replace('\r\n', '\n')

        with open(output_file(test.name(), self.__output_directory), mode='w') as out_file:
            out_file.write(out)

        return TestResult(test, out)


class TestRepository:
    def __init__(self, path: str) -> None:
        assert os.path.exists(path), "Test repository is not found"
        assert os.path.isdir(path), "Test repository must be a directory"
        self.__path = path
        self.__ignored_dirs = {'common', 'memory'}

    def test_count(self) -> int:
        return len(list(self.__iterate_tests_files(self.test_src_dir())))

    def read_output(self, name) -> Optional[str]:
        try:
            with open(output_file(name, self.__test_out_dir()), mode='r') as file:
                return file.read()
        except IOError:
            return None

    def write_output(self, name, output):
        with open(output_file(name, self.__test_out_dir()), mode='w') as file:
            file.write(output)

    def iterate_tests(self) -> Iterable[Test]:
        src_dir = self.test_src_dir()
        for test_name in self.__iterate_tests_files(src_dir):
            yield Test(test_name, self.read_output(test_name), src_dir)

    def __iterate_tests_files(self, src_dir, package: str = '') -> Iterable[str]:
        for file_name in os.listdir(src_dir):
            path = os.path.join(src_dir, file_name)
            if os.path.isdir(path):
                if not self.__is_ignored_dir(file_name):
                    yield from self.__iterate_tests_files(path, self.__join_with_package(package, file_name))
            else:
                yield self.__join_with_package(package, str(file_name).split('.java')[0])

    def test_src_dir(self) -> str:
        return os.path.join(self.__path, 'src')

    def get_all_files_for_compilation(self) -> List[str]:
        result = self.__list_files_from_src_root(os.path.join(self.__path, 'src'))
        if PROXY_COMPILED_PATH is None:
            result.extend(self.__list_files_from_src_root(os.path.join(self.__path, 'proxy/src')))
        return result

    @staticmethod
    def __list_files_from_src_root(src_root) -> List[str]:
        result = list()
        for root, dirs, files in os.walk(src_root):
            for file in files:
                if file != ".DS_Store":
                    result.append(os.path.join(root, file))
        return result

    @staticmethod
    def __join_with_package(parent: str, child: str) -> str:
        return child if parent == '' else '{}.{}'.format(parent, child)

    def __test_out_dir(self) -> str:
        return os.path.join(self.__path, 'outs')

    def __is_ignored_dir(self, dir_name: str) -> bool:
        return dir_name in self.__ignored_dirs


test_repo = TestRepository('test_data')
timestamp = int(time.time())
timestamp = datetime.fromtimestamp(timestamp).strftime('%Y.%m.%d_%H.%M.%S')
build_directory = 'test_outs/{}/build'.format(timestamp)
output_directory = 'test_outs/{}/outs'.format(timestamp)


class NativeAgentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        os.makedirs(build_directory)
        os.makedirs(output_directory)
        JavaCompiler(get_java_compiler(), build_directory) \
            .compile_java(test_repo.get_all_files_for_compilation(), PROXY_COMPILED_PATH)
        if PROXY_COMPILED_PATH is not None:
            distutils.dir_util.copy_tree(PROXY_COMPILED_PATH, build_directory)


def to_test_name(value: str) -> str:
    return 'test_{}'.format(value.replace('.', '_').replace(' ', '_').lower())


def create_test(test: Test, runner: TestRunner, repo: TestRepository):
    def do_test(self: TestCase):
        try:
            result = runner.run(test)
        except CalledProcessError as ex:
            self.fail(ex.output)

        actual = result.get_output()
        expected = test.expected_output()
        if expected is not None:
            self.assertEqual(expected.strip(), actual.strip(), "outputs are mismatched")
        else:
            repo.write_output(test.name(), actual)
            error_text = "********* EXPECTED OUTPUT NOT FOUND. DO NOT FORGET PUT IT INTO VCS *********"
            self.fail(error_text)

    return do_test


def create_tests():
    runner = TestRunner(get_java_executable(), build_directory, output_directory, find_agent_file())

    for test in test_repo.iterate_tests():
        setattr(NativeAgentTests, to_test_name(test.name()), create_test(test, runner, test_repo))


if __name__ == '__main__':
    create_tests()
    if IS_UNDER_TEAMCITY:
        runner = TeamcityTestRunner()
    else:
        runner = unittest.TextTestRunner()
    unittest.main(testRunner=runner)
