import os
import sys
import time
import unittest
from datetime import datetime
from subprocess import check_output
from typing import Iterable, Optional, List


class JavaCompiler:
    def __init__(self, javac: str, output_path: str) -> None:
        self.__output = output_path
        self.__javac = javac

    def compile_java(self, source_files: List[str]):
        args = list()
        args.append(self.__javac)
        args.extend(['-d', self.__output])
        args.extend(source_files)
        check_output(args)


class Test:
    def __init__(self, name: str, output: str, src_dir: str) -> None:
        self.__name = name
        self.__output = output
        self.__path = '{}/{}'.format(src_dir, name)

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
    def __init__(self, java, build_dir: str, output_dir: str) -> None:
        self.__java = java
        self.__build_dir = build_dir
        self.__output_directory = output_dir

    def run(self, test: Test) -> TestResult:
        if not os.path.exists(self.__output_directory):
            os.makedirs(self.__output_directory)

        args = list()
        args.append(self.__java)
        args.append('-agentpath:./cmake-build-debug/native_memory_agent.dll')
        args.extend(['-classpath', self.__build_dir])
        args.append(test.name())

        out = check_output(args).decode("utf-8").replace('\r\n', '\n')

        with open('{}/{}.out'.format(self.__output_directory, test.name()), mode='w') as out_file:
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
            with open('{}/{}.out'.format(self.__test_out_dir(), name), mode='r') as file:
                return file.read()
        except IOError:
            return None

    def write_output(self, name, output):
        with open('{}/{}.out'.format(self.__test_out_dir(), name), mode='w') as file:
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
                yield self.__join_with_package(package, file_name.split('.java')[0])

    def test_src_dir(self) -> str:
        return os.path.join(self.__path, 'src')

    def get_all_files_for_compilation(self) -> List[str]:
        result = list()
        for root, dirs, files in os.walk(os.path.join(self.__path, 'src')):
            for file in files:
                result.append(os.path.join(root, file))
        return result

    @staticmethod
    def __join_with_package(parent: str, child: str) -> str:
        return child if parent == '' else '{}.{}'.format(parent, child)

    def __test_out_dir(self) -> str:
        return os.path.join(self.__path, 'outs')

    def __is_ignored_dir(self, dir_name: str) -> bool:
        return dir_name in self.__ignored_dirs


class TestResultAggregator:
    def __init__(self) -> None:
        self.__success = list()
        self.__failed = list()

    def test_finished(self, test: Test, test_result: TestResult):
        actual = test_result.get_output()
        expected = test.expected_output()
        if expected is None:
            self.__failed.append((test, "Expected output not found"))
        elif actual.strip() != expected.strip():
            self.__failed.append((test, "Actual and excepted outputs not matched"))
        else:
            self.__success.append(test)

    def report_stats(self) -> int:
        failed_count = len(self.__failed)
        if failed_count == 0:
            print("ALL TESTS PASSED")
        else:
            print("{} tests failed".format(len(self.__failed)))
            for test, failure_description in self.__failed:
                print("{}: {}".format(test.name(), failure_description))
            print()
            print("TESTS FAILED")
        return failed_count


# class NativeAgentTests(unittest.TestCase):
#     pass


def main():
    test_repo = TestRepository('test_data')
    timestamp = int(time.time())
    timestamp = datetime.fromtimestamp(timestamp).strftime('%Y.%m.%d_%H.%M.%S')
    build_directory = 'test_outs/{}/build'.format(timestamp)
    output_directory = 'test_outs/{}/outs'.format(timestamp)
    os.makedirs(build_directory)
    os.makedirs(output_directory)
    JavaCompiler("C:\\Program Files\\Java\\jdk1.8.0_151\\bin\\javac.exe", build_directory) \
        .compile_java(test_repo.get_all_files_for_compilation())
    aggregator = TestResultAggregator()
    runner = TestRunner("C:\\Program Files\\Java\\jdk1.8.0_151\\bin\\java.exe", build_directory, output_directory)
    print('{} tests found.'.format(test_repo.test_count()))
    for test in test_repo.iterate_tests():
        print("running test: {}".format(test.name()))
        result = runner.run(test)
        aggregator.test_finished(test, result)
    failed_count = aggregator.report_stats()
    sys.exit(failed_count)


if __name__ == "__main__":
    main()
