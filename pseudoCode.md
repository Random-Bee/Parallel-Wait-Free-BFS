```
input: G(V, E), s
output: array d[1..n] of distances from s to all other vertices

class circularLinkedList {
    int data;
    circularLinkedList* next;
};

class LinkedList {
    circularLinkedList* itr;
    circularLinkedList* tail;
    LinkedList* next;

    circularLinkedList* getCurrNode() {
        get currernt node and move itr to next node
    }

    void insertNode(int data) {
        insert node at the end of the circular linked list
    }

    void deleteNode(circularLinkedList* node) {
        delete the node from the circular linked list
    }

    bool isEmpty() {
        return true if the circular linked list is empty
    }
};

int visited[1..n] = {0};
int d[1..n] = {-1};

void bfs(LinkedList* curr) {
    while(1) {
        if(curr->isEmpty()) {
            // Two cases:
            // 1. Traversal is complete
            // 2. When this thread was working on the last node of the previous level, some fast thread has already completed the traversal at this level
            if(curr->next == NULL) {
                // In case 2, if some thread has already completed the traversal at this level, then that thread will add a node for the next level.
                // So if the next level is empty, then the traversal must be complete.

                return;
            }
            curr = curr->next;
        }
        if(curr->next == NULL) {
            LinkedList* newLevel = new LinkedList();
            compare_and_swap(curr->next, NULL, newLevel);
        }
        while(!curr->isEmpty()) {
            circularLinkedList* node = curr->getCurrNode();
            int neighours[] = G[node->data];
            shuffle(neighours);
            for(int i = 0; i < neighours.size(); i++) {
                if(d[neighours[i]] == -1) {
                    curr->next->insertNode(neighours[i]);
                    d[neighours[i]] = d[node->data] + 1;
                }
            }
            curr->deleteNode(node);
        }
    }
}
```