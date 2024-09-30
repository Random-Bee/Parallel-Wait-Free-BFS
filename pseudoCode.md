```
input: G(V, E), s
output: array d[1..n] of distances from s to all other vertices

class waitFreeLinkedList {
    int data;
    waitFreeLinkedList* next;
};

t = number of threads

class outerLinkedList {
    waitFreeLinkedList* heads[t]
    int curr_list = 0;

    void insertNode(int data, int list_num) {
        // Insert node at any position in the list at index list_num
    }
};

int visited[1..n] = {0};
int d[1..n] = {-1};

void bfs(outerLinkedList* curr) {
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
            outerLinkedList* newLevel = new outerLinkedList();
            compare_and_swap(curr->next, NULL, newLevel);
        }

        int null_count = 0;

        while(1) {
            // Get list_num to operate on
            int list_num = fetch_add(curr_list, 1) % t;

            // Iterate in the list at index list_num
            waitFreeLinkedList* Head = heads[list_num];
            waitFreeLinkedList* temp = Head;

            if(temp == NULL) {
                null_count++;
                if(null_count == t) {
                    // All lists are empty
                    break;
                }
                continue;
            }

            null_count = 0;

            while(temp != NULL) {
                int neighours[] = G[temp->data];
                shuffle(neighours);
                for(int i = 0; i < neighours.size(); i++) {
                    if(d[neighours[i]] == -1) {
                        // get list_num of the next level
                        int next_list_num = fetch_add(curr->next->curr_list, 1) % t;
                        curr->next->insertNode(neighours[i], next_list_num);
                        d[neighours[i]] = d[temp->data] + 1;
                    }
                }
                temp = temp->next;
            }

            curr->heads[list_num] = NULL;
        }
    }
}
```