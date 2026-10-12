#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <kdtree.h>

static int sort_axis = 0;

static Node *create_node(const double *point, int is_fraud)
{
    Node *node = malloc(sizeof(*node));
    if (node == NULL)
        return NULL;

    for (int i = 0; i < DIMENSIONS; i++)
        node->point[i] = point[i];

    node->is_fraud = (unsigned char)(is_fraud != 0);
    node->left = NULL;
    node->right = NULL;
    return node;
}

static int compare_references(const void *a, const void *b)
{
    const Reference *ref_a = a;
    const Reference *ref_b = b;

    if (ref_a->point[sort_axis] < ref_b->point[sort_axis])
        return -1;
    if (ref_a->point[sort_axis] > ref_b->point[sort_axis])
        return 1;
    return 0;
}

Node *build_tree(Reference *references, int num_points, int depth)
{
    if (num_points <= 0)
        return NULL;

    sort_axis = depth % DIMENSIONS;
    qsort(references, (size_t)num_points, sizeof(*references), compare_references);

    int mid = num_points / 2;

    Node *root = create_node(
        references[mid].point,
        references[mid].is_fraud);

    if (root == NULL)
        return NULL;

    root->left = build_tree(references, mid, depth + 1);
    root->right = build_tree(
        references + mid + 1,
        num_points - mid - 1,
        depth + 1);

    return root;
}

static double squared_distance(const double *a, const double *b)
{
    double sum = 0.0;
    for (int i = 0; i < DIMENSIONS; i++)
    {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

static void insert_candidate(Candidate best[K_NEIGHBORS], int *count,
                             Node *node, double distance)
{
    int pos;
    if (*count < K_NEIGHBORS)
    {
        pos = (*count)++;
    }
    else
    {
        if (distance >= best[K_NEIGHBORS - 1].distance)
            return;
        pos = K_NEIGHBORS - 1;
    }

    while (pos > 0 && distance < best[pos - 1].distance)
    {
        best[pos] = best[pos - 1];
        pos--;
    }
    best[pos].node = node;
    best[pos].distance = distance;
}

static void search_5_recursive(Node *root, const double *target, int depth,
                               Candidate best[K_NEIGHBORS], int *count)
{
    if (root == NULL)
        return;

    insert_candidate(best, count, root, squared_distance(root->point, target));

    int axis = depth % DIMENSIONS;
    double delta = target[axis] - root->point[axis];
    Node *near_child = delta < 0.0 ? root->left : root->right;
    Node *far_child = delta < 0.0 ? root->right : root->left;

    search_5_recursive(near_child, target, depth + 1, best, count);

    /* Until there are 5 candidates, the far side cannot be safely pruned. */
    double worst_distance = *count < K_NEIGHBORS
                                ? DBL_MAX
                                : best[K_NEIGHBORS - 1].distance;
    if (delta * delta <= worst_distance)
        search_5_recursive(far_child, target, depth + 1, best, count);
}

int search_5_nearest_neighbors(Node *root, const double *target,
                               Candidate best[K_NEIGHBORS])
{
    int count = 0;
    search_5_recursive(root, target, 0, best, &count);
    return count;
}

void destroy_tree(Node *root)
{
    if (root == NULL)
        return;
    destroy_tree(root->left);
    destroy_tree(root->right);
    free(root);
}
