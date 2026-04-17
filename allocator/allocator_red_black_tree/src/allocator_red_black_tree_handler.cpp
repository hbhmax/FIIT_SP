#include "allocator_red_black_tree_detail.h"

namespace allocator_red_black_tree_detail
{
    void transplant(void* u, void* v, void* trusted)
    {
        if (parent_block_ref(u) == nullptr)
        {
            root_ref(trusted) = v;
        }
        else if (left_child_block_ref(parent_block_ref(u)) == u)
        {
            left_child_block_ref(parent_block_ref(u)) = v;
        }
        else
        {
            right_child_block_ref(parent_block_ref(u)) = v;
        }

        if (v != nullptr)
        {
            parent_block_ref(v) = parent_block_ref(u);
        }
    }

    void rotate_left(void* x, void* trusted)
    {
        if (x == nullptr || x == root_ref(trusted) || parent_block_ref(x) == nullptr)
            return;

        void* tmp = left_child_block_ref(x);
        left_child_block_ref(x) = parent_block_ref(x);
        right_child_block_ref(parent_block_ref(x)) = tmp;
        if (tmp != nullptr)
        {
            parent_block_ref(tmp) = parent_block_ref(x);
        }

        transplant(parent_block_ref(x), x, trusted);
        parent_block_ref(left_child_block_ref(x)) = x;
    }

    void rotate_right(void* x, void* trusted)
    {
        if (x == nullptr || x == root_ref(trusted) || parent_block_ref(x) == nullptr)
            return;

        void* tmp = right_child_block_ref(x);
        right_child_block_ref(x) = parent_block_ref(x);
        left_child_block_ref(parent_block_ref(x)) = tmp;
        if (tmp != nullptr)
        {
            parent_block_ref(tmp) = parent_block_ref(x);
        }

        transplant(parent_block_ref(x), x, trusted);
        parent_block_ref(right_child_block_ref(x)) = x;
    }

    void* grandparent(void* node)
    {
        void* parent = parent_block_ref(node);
        if (parent && parent_block_ref(parent))
            return parent_block_ref(parent);
        return nullptr;
    }

    void* uncle(void* node)
    {
        void* gp = grandparent(node);
        if (gp == nullptr)
            return nullptr;
        if (parent_block_ref(node) == left_child_block_ref(gp))
            return right_child_block_ref(gp);
        return left_child_block_ref(gp);
    }

    bool is_right_child(void* node)
    {
        if (node == nullptr) return false;
        if (parent_block_ref(node) == nullptr) return false;
        return right_child_block_ref(parent_block_ref(node)) == node;
    }

    bool is_left_child(void* node)
    {
        if (node == nullptr) return false;
        if (parent_block_ref(node) == nullptr) return false;
        return left_child_block_ref(parent_block_ref(node)) == node;
    }

    void on_node_added(void* block, void* trusted)
    {
        insert_case_1(block, trusted);
    }

    void insert_case_1(void* node, void* trusted)
    {
        if (parent_block_ref(node) == nullptr)
            block_data_ref(node).color = block_color::BLACK;
        else
            insert_case_2(node, trusted);
    }

    void insert_case_2(void* node, void* trusted)
    {
        if (block_data_ref(parent_block_ref(node)).color == block_color::RED)
            insert_case_3(node, trusted);
    }

    void insert_case_3(void* node, void* trusted)
    {
        void* unc = uncle(node);
        if (unc != nullptr && block_data_ref(unc).color == block_color::RED)
        {
            block_data_ref(parent_block_ref(node)).color = block_color::BLACK;
            block_data_ref(unc).color = block_color::BLACK;
            void* gp = grandparent(node);
            block_data_ref(gp).color = block_color::RED;
            insert_case_1(gp, trusted);
        }
        else
        {
            insert_case_4(node, trusted);
        }
    }

    void insert_case_4(void* node, void* trusted)
    {
        if (is_right_child(node) && is_left_child(parent_block_ref(node)))
        {
            rotate_left(node, trusted);
            node = left_child_block_ref(node);
        }
        else if (is_left_child(node) && is_right_child(parent_block_ref(node)))
        {
            rotate_right(node, trusted);
            node = right_child_block_ref(node);
        }
        insert_case_5(node, trusted);
    }

    void insert_case_5(void* node, void* trusted)
    {
        void* parent = parent_block_ref(node);
        void* gp = grandparent(node);

        block_data_ref(parent).color = block_color::BLACK;
        block_data_ref(gp).color = block_color::RED;

        if (is_left_child(node) && is_left_child(parent))
            rotate_right(parent, trusted);
        else
            rotate_left(parent, trusted);
    }

    void add_node(void* block, void* trusted)
    {
        if (root_ref(trusted) == nullptr)
        {
            root_ref(trusted) = block;
            parent_block_ref(block) = nullptr;
            left_child_block_ref(block) = nullptr;
            right_child_block_ref(block) = nullptr;
            on_node_added(block, trusted);
            return;
        }

        void* current = root_ref(trusted);
        while (true)
        {
            if (block_size_of(block, trusted) <= block_size_of(current, trusted))
            {
                if (left_child_block_ref(current) != nullptr)
                {
                    current = left_child_block_ref(current);
                }
                else
                {
                    left_child_block_ref(current) = block;
                    parent_block_ref(block) = current;
                    left_child_block_ref(block) = nullptr;
                    right_child_block_ref(block) = nullptr;
                    on_node_added(block, trusted);
                    return;
                }
            }
            else
            {
                if (right_child_block_ref(current) != nullptr)
                {
                    current = right_child_block_ref(current);
                }
                else
                {
                    right_child_block_ref(current) = block;
                    parent_block_ref(block) = current;
                    left_child_block_ref(block) = nullptr;
                    right_child_block_ref(block) = nullptr;
                    on_node_added(block, trusted);
                    return;
                }
            }
        }
    }

    void on_node_removed(void* parent, void* child, block_color deleted_color, void* trusted)
    {
        if (deleted_color == block_color::BLACK)
        {
            if (child != nullptr && block_color_of(child) == block_color::RED)
            {
                block_data_ref(child).color = block_color::BLACK;
            }
            else
            {
                remove_case_1(parent, child, trusted);
            }
        }
    }

    void remove_case_1(void* parent, void* child, void* trusted)
    {
        if (parent != nullptr)
            remove_case_2(parent, child, trusted);
    }

    void* sibling(void* parent, void* child)
    {
        if (left_child_block_ref(parent) == child)
            return right_child_block_ref(parent);
        return left_child_block_ref(parent);
    }

    void remove_case_2(void* parent, void* child, void* trusted)
    {
        void* sib = sibling(parent, child);

        if (sib != nullptr && block_color_of(sib) == block_color::RED)
        {
            block_data_ref(parent).color = block_color::RED;
            block_data_ref(sib).color = block_color::BLACK;
            if (is_left_child(sib))
                rotate_right(sib, trusted);
            else
                rotate_left(sib, trusted);
        }
        remove_case_3(parent, child, trusted);
    }

    void remove_case_3(void* parent, void* child, void* trusted)
    {
        void* sib = sibling(parent, child);
        if ((block_color_of(parent) == block_color::BLACK)
            && (block_color_of(sib) == block_color::BLACK)
            && (left_child_block_ptr(sib) == nullptr || block_color_of(left_child_block_ref(sib)) == block_color::BLACK)
            && (right_child_block_ptr(sib) == nullptr || block_color_of(right_child_block_ref(sib)) == block_color::BLACK))
        {
            block_data_ref(sib).color = block_color::RED;
            remove_case_1(parent_block_ref(parent), parent, trusted);
        }
        else
        {
            remove_case_4(parent, child, trusted);
        }
    }

    void remove_case_4(void* parent, void* child, void* trusted)
    {
        void* sib = sibling(parent, child);
        if ((block_color_of(parent) == block_color::RED)
            && (block_color_of(sib) == block_color::BLACK)
            && (left_child_block_ptr(sib) == nullptr || block_color_of(left_child_block_ref(sib)) == block_color::BLACK)
            && (right_child_block_ptr(sib) == nullptr || block_color_of(right_child_block_ref(sib)) == block_color::BLACK))
        {
            block_data_ref(sib).color = block_color::RED;
            block_data_ref(parent).color = block_color::BLACK;
        }
        else
        {
            remove_case_5(parent, child, trusted);
        }
    }

    void remove_case_5(void* parent, void* child, void* trusted)
    {
        void* sib = sibling(parent, child);
        if (block_color_of(sib) == block_color::BLACK)
        {
            if (is_right_child(sib)
                && (left_child_block_ptr(sib) != nullptr && block_color_of(left_child_block_ref(sib)) == block_color::RED)
                && (right_child_block_ptr(sib) == nullptr || block_color_of(right_child_block_ref(sib)) == block_color::BLACK))
            {
                block_data_ref(sib).color = block_color::RED;
                block_data_ref(left_child_block_ref(sib)).color = block_color::BLACK;
                rotate_right(left_child_block_ref(sib), trusted);
            }
            else if (is_left_child(sib)
                && (left_child_block_ptr(sib) == nullptr || block_color_of(left_child_block_ref(sib)) == block_color::BLACK)
                && (right_child_block_ptr(sib) != nullptr && block_color_of(right_child_block_ref(sib)) == block_color::RED))
            {
                block_data_ref(sib).color = block_color::RED;
                block_data_ref(right_child_block_ref(sib)).color = block_color::BLACK;
                rotate_left(right_child_block_ref(sib), trusted);
            }
        }
        remove_case_6(parent, child, trusted);
    }

    void remove_case_6(void* parent, void* child, void* trusted)
    {
        void* sib = sibling(parent, child);
        block_data_ref(sib).color = block_color_of(parent);
        block_data_ref(parent).color = block_color::BLACK;

        if (is_right_child(sib))
        {
            block_data_ref(right_child_block_ref(sib)).color = block_color::BLACK;
            rotate_left(sib, trusted);
        }
        else
        {
            block_data_ref(left_child_block_ref(sib)).color = block_color::BLACK;
            rotate_right(sib, trusted);
        }
    }

    void* find_rightmost(void* node)
    {
        void* prev = node;
        void* cur = node;
        while (cur != nullptr)
        {
            prev = cur;
            cur = right_child_block_ref(cur);
        }
        return prev;
    }

    void remove_node(void* block, void* trusted)
    {
        if (left_child_block_ref(block) == nullptr && right_child_block_ref(block) == nullptr)
        {
            transplant(block, nullptr, trusted);
            on_node_removed(parent_block_ref(block), nullptr, block_color_of(block), trusted);
            return;
        }

        if (left_child_block_ref(block) == nullptr)
        {
            transplant(block, right_child_block_ref(block), trusted);
            on_node_removed(parent_block_ref(block), right_child_block_ref(block), block_color_of(block), trusted);
            return;
        }

        if (right_child_block_ref(block) == nullptr)
        {
            transplant(block, left_child_block_ref(block), trusted);
            on_node_removed(parent_block_ref(block), left_child_block_ref(block), block_color_of(block), trusted);
            return;
        }

        void* replacement = find_rightmost(left_child_block_ref(block));

        void* replacement_parent =
            parent_block_ref(replacement) == block ? replacement : parent_block_ref(replacement);

        void* replacement_for_replacement = left_child_block_ref(replacement);
        block_color deleted_color = block_color_of(replacement);

        transplant(replacement, replacement_for_replacement, trusted);

        right_child_block_ref(replacement) = right_child_block_ref(block);
        if (right_child_block_ref(replacement) != nullptr)
            parent_block_ref(right_child_block_ref(replacement)) = replacement;

        left_child_block_ref(replacement) = left_child_block_ref(block);
        if (left_child_block_ref(replacement) != nullptr)
            parent_block_ref(left_child_block_ref(replacement)) = replacement;

        block_data_ref(replacement).color = block_data_ref(block).color;
        transplant(block, replacement, trusted);

        on_node_removed(replacement_parent, replacement_for_replacement, deleted_color, trusted);
    }
}