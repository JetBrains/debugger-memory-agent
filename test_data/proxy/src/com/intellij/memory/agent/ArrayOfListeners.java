package com.intellij.memory.agent;

class ArrayOfListeners {
    public Object[] listenerHolders;

    ArrayOfListeners() {
        listenerHolders = new Object[0];
    }

    void add(AllocationListener allocationListener, Class<?>[] trackedClasses) {
        int size = listenerHolders.length;
        Object[] temp = new Object[size + 1];
        if (size != 0) {
            System.arraycopy(listenerHolders, 0, temp, 0, size);
        }
        listenerHolders = temp;
        listenerHolders[size] = new AllocationListenerHolder(allocationListener, trackedClasses);
    }

    void remove(AllocationListener allocationListener) {
        remove(findListener(allocationListener));
    }

    void remove(int index) {
        int newSize = listenerHolders.length - 1;
        if (index < 0 || index > newSize) {
            return;
        }

        Object[] temp = new Object[newSize];
        System.arraycopy(listenerHolders, 0, temp, 0, index);
        System.arraycopy(listenerHolders, index + 1, temp, index, newSize - index);
        listenerHolders = temp;
    }

    private int findListener(AllocationListener allocationListener) {
        for (int i = 0; i < listenerHolders.length; i++) {
            if (((AllocationListenerHolder)listenerHolders[i]).listener.equals(allocationListener)) {
                return i;
            }
        }

        return -1;
    }
}
