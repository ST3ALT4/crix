#include <stdio.h>
#include <stdlib.h>

struct Node {
    int val;
    struct Node *next;
};

struct Node *create_node(int val) {
    struct Node *n = (struct Node *)malloc(sizeof(struct Node));
    n->val = val;
    n->next = NULL;
    return n;
}

int main(void) {
    struct Node *head = create_node(10);
    struct Node *second = create_node(20);
    struct Node *third = create_node(30);

    head->next = second;
    second->next = third;

    struct Node *curr = head;
    int sum = 0;
    while (curr != NULL) {
        sum += curr->val;
        curr = curr->next;
    }

    printf("Sum of linked list: %d\n", sum);

    free(third);
    free(second);
    free(head);
    return 0;
}
