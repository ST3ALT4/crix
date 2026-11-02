#include <stdio.h>
#include <stdlib.h>

struct TreeNode {
    int val;
    struct TreeNode *left;
    struct TreeNode *right;
};

struct TreeNode *make_node(int val) {
    struct TreeNode *n = (struct TreeNode *)malloc(sizeof(struct TreeNode));
    n->val = val;
    n->left = NULL;
    n->right = NULL;
    return n;
}

int main(void) {
    struct TreeNode *root = make_node(50);
    root->left = make_node(25);
    root->right = make_node(75);

    int root_val = root->val;
    int left_val = root->left->val;
    int right_val = root->right->val;

    printf("Tree: root=%d, left=%d, right=%d\n", root_val, left_val, right_val);

    free(root->right);
    free(root->left);
    free(root);
    return 0;
}
