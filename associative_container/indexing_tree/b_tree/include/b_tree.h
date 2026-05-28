#ifndef SYS_PROG_B_TREE_H
#define SYS_PROG_B_TREE_H

#include <iterator>
#include <utility>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <vector>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list>

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class B_tree final : private compare // EBCO
{
public:
    // region exceptions
    class btree_exception : public std::exception {
        std::string _msg;
    public:
        explicit btree_exception(std::string msg) : _msg(std::move(msg)) {}
        const char* what() const noexcept override { return _msg.c_str(); }
    };

    class out_of_range : public btree_exception {
    public:
        explicit out_of_range(const std::string& msg) : btree_exception(msg) {}
    };
    // endregion exceptions

    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:
    struct btree_node;

    using node_allocator_traits = std::allocator_traits<pp_allocator<value_type>>;
    using node_allocator_type = typename node_allocator_traits::template rebind_alloc<btree_node>;

    btree_node* create_node() {
        node_allocator_type node_alloc(_allocator);
        btree_node* node = std::allocator_traits<node_allocator_type>::allocate(node_alloc, 1);
        std::allocator_traits<node_allocator_type>::construct(node_alloc, node);
        return node;
    }

    void destroy_node(btree_node* node) {
        if (!node) return;
        node_allocator_type node_alloc(_allocator);
        std::allocator_traits<node_allocator_type>::destroy(node_alloc, node);
        std::allocator_traits<node_allocator_type>::deallocate(node_alloc, node, 1);
    }

    btree_node* clone_node(btree_node* other_node);
    void clear_node(btree_node* node) noexcept;

    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // region comparators declaration

    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;

    // endregion comparators declaration

    struct btree_node
    {
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<btree_node*, maximum_keys_in_node + 2> _pointers;
        btree_node() noexcept;
    };

    pp_allocator<value_type> _allocator;
    btree_node* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

public:

    // region constructors declaration

    explicit B_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit B_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit B_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    B_tree(const B_tree& other);

    B_tree(B_tree&& other) noexcept;

    B_tree& operator=(const B_tree& other);

    B_tree& operator=(B_tree&& other) noexcept;

    ~B_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class btree_iterator;
    class btree_reverse_iterator;
    class btree_const_iterator;
    class btree_const_reverse_iterator;

    class btree_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);

    };

    class btree_const_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_iterator;
        friend class btree_const_reverse_iterator;

        btree_const_iterator(const btree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    class btree_reverse_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_reverse_iterator;

        friend class B_tree;
        friend class btree_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        btree_reverse_iterator(const btree_iterator& it) noexcept;
        operator btree_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_reverse_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);
    };

    class btree_const_reverse_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_reverse_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_iterator;

        btree_const_reverse_iterator(const btree_reverse_iterator& it) noexcept;
        operator btree_const_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_reverse_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;
    friend class btree_reverse_iterator;
    friend class btree_const_reverse_iterator;

    // endregion iterators declaration

    // region element access declaration

    tvalue& at(const tkey&);
    const tvalue& at(const tkey&) const;

    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration

    // region iterator begins declaration

    btree_iterator begin();
    btree_iterator end();

    btree_const_iterator begin() const;
    btree_const_iterator end() const;

    btree_const_iterator cbegin() const;
    btree_const_iterator cend() const;

    btree_reverse_iterator rbegin();
    btree_reverse_iterator rend();

    btree_const_reverse_iterator rbegin() const;
    btree_const_reverse_iterator rend() const;

    btree_const_reverse_iterator crbegin() const;
    btree_const_reverse_iterator crend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    btree_iterator find(const tkey& key);
    btree_const_iterator find(const tkey& key) const;

    btree_iterator lower_bound(const tkey& key);
    btree_const_iterator lower_bound(const tkey& key) const;

    btree_iterator upper_bound(const tkey& key);
    btree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    std::pair<btree_iterator, bool> insert(const tree_data_type& data);
    std::pair<btree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<btree_iterator, bool> emplace(Args&&... args);

    btree_iterator insert_or_assign(const tree_data_type& data);
    btree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    btree_iterator emplace_or_assign(Args&&... args);

    btree_iterator erase(btree_iterator pos);
    btree_iterator erase(btree_const_iterator pos);

    btree_iterator erase(btree_iterator beg, btree_iterator en);
    btree_iterator erase(btree_const_iterator beg, btree_const_iterator en);

    btree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
B_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_pairs(const B_tree::tree_data_type &lhs,
                                                     const B_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_node::btree_node() noexcept
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename B_tree<tkey, tvalue, compare, t>::value_type> B_tree<tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

// region constructors implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        pp_allocator<value_type> alloc,
        const compare& comp)
    : compare(comp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
B_tree<tkey, tvalue, compare, t>::B_tree(
        iterator begin,
        iterator end,
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : B_tree(cmp, alloc)
{
    for (auto it = begin; it != end; ++it)
    {
        insert(*it);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        std::initializer_list<std::pair<tkey, tvalue>> data,
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : B_tree(cmp, alloc)
{
    for (const auto& item : data)
    {
        insert(item);
    }
}

// endregion constructors implementation

// region five implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::~B_tree() noexcept
{
    clear();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(const B_tree& other)
    : compare(other),
      _allocator(other._allocator),
      _root(clone_node(other._root)),
      _size(other._size)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(const B_tree& other)
{
    if (this != &other) {
        B_tree temp(other);

        std::swap(_root, temp._root);
        std::swap(_size, temp._size);
        std::swap(_allocator, temp._allocator);
        std::swap(static_cast<compare&>(*this), static_cast<compare&>(temp));
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(B_tree&& other) noexcept
    : compare(std::move(other)),
      _allocator(std::move(other._allocator)),
      _root(other._root),
      _size(other._size)
{
    other._root = nullptr;
    other._size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(B_tree&& other) noexcept
{
    if (this != &other) {
        clear();

        compare::operator=(std::move(other));
        _allocator = std::move(other._allocator);
        _root = other._root;
        _size = other._size;

        other._root = nullptr;
        other._size = 0;
    }

    return *this;
}

// endregion five implementation

// region iterators implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_iterator::btree_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
    : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator*() const noexcept
{
    btree_node* curr = *_path.top().first;
    return reinterpret_cast<reference>(curr->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }

    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index + 1;
        btree_node** next_child = &(curr->_pointers[_index + 1]);
        while (*next_child) {
            _path.push({next_child, 0});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[0]);
        }
        _index = 0;
    }
    else {
        if (_index + 1 < curr->_keys.size()) {
            _index++;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                btree_node* parent = *parent_ptr;
                if (child_idx < parent->_keys.size()) {
                    _index = child_idx;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++(int)
{
    self tmp = *this;
    ++(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index;
        btree_node** next_child = &(curr->_pointers[_index]);
        while (*next_child) {
            size_t last_ptr_idx = (*next_child)->_pointers.empty() ? 0 : (*next_child)->_pointers.size() - 1;
            _path.push({next_child, last_ptr_idx});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[last_ptr_idx]);
        }
        _index = (*next_child)->_keys.size() - 1;
    }
    else {
        if (_index > 0) {
            _index--;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                if (child_idx > 0) {
                    _index = child_idx - 1;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--(int)
{
    self tmp = *this;
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty()) return true;
    if (_path.empty() || other._path.empty()) return false;
    return (*_path.top().first == *other._path.top().first) && (_index == other._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) return 0;
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) return true;
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
    : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const btree_iterator& it) noexcept
    : _index(it._index)
{
    auto temp_stack = it._path;
    std::vector<std::pair<btree_node* const*, size_t>> elems;
    while (!temp_stack.empty()) {
        auto [node_ptr, idx] = temp_stack.top();
        temp_stack.pop();
        elems.push_back({reinterpret_cast<btree_node* const*>(node_ptr), idx});
    }
    for (auto it_elem = elems.rbegin(); it_elem != elems.rend(); ++it_elem) {
        _path.push(*it_elem);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator*() const noexcept
{
    btree_node* curr = *_path.top().first;
    return reinterpret_cast<reference>(curr->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }

    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index + 1;
        btree_node* const* next_child = &(curr->_pointers[_index + 1]);
        while (*next_child) {
            _path.push({next_child, 0});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[0]);
        }
        _index = 0;
    }
    else {
        if (_index + 1 < curr->_keys.size()) {
            _index++;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                btree_node* parent = *parent_ptr;
                if (child_idx < parent->_keys.size()) {
                    _index = child_idx;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++(int)
{
    self tmp = *this;
    ++(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index;
        btree_node* const* next_child = &(curr->_pointers[_index]);
        while (*next_child) {
            size_t last_ptr_idx = (*next_child)->_pointers.empty() ? 0 : (*next_child)->_pointers.size() - 1;
            _path.push({next_child, last_ptr_idx});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[last_ptr_idx]);
        }
        _index = (*next_child)->_keys.size() - 1;
    }
    else {
        if (_index > 0) {
            _index--;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                if (child_idx > 0) {
                    _index = child_idx - 1;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--(int)
{
    self tmp = *this;
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty()) return true;
    if (_path.empty() || other._path.empty()) return false;
    return (*_path.top().first == *other._path.top().first) && (_index == other._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) return 0;
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) return true;
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
    : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const btree_iterator& it) noexcept
    : _path(it._path), _index(it._index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_iterator() const noexcept
{
    return btree_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator*() const noexcept
{
    btree_node* curr = *_path.top().first;
    return reinterpret_cast<reference>(curr->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++()
{
    if (_path.empty()) return *this;

    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index;
        btree_node** next_child = &(curr->_pointers[_index]);
        while (*next_child) {
            size_t last_ptr_idx = (*next_child)->_pointers.empty() ? 0 : (*next_child)->_pointers.size() - 1;
            _path.push({next_child, last_ptr_idx});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[last_ptr_idx]);
        }
        _index = (*next_child)->_keys.size() - 1;
    }
    else {
        if (_index > 0) {
            _index--;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                if (child_idx > 0) {
                    _index = child_idx - 1;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++(int)
{
    self tmp = *this;
    ++(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--()
{
    if (_path.empty()) return *this;

    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index + 1;
        btree_node** next_child = &(curr->_pointers[_index + 1]);
        while (*next_child) {
            _path.push({next_child, 0});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[0]);
        }
        _index = 0;
    }
    else {
        if (_index + 1 < curr->_keys.size()) {
            _index++;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                btree_node* parent = *parent_ptr;
                if (child_idx < parent->_keys.size()) {
                    _index = child_idx;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--(int)
{
    self tmp = *this;
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty()) return true;
    if (_path.empty() || other._path.empty()) return false;
    return (*_path.top().first == *other._path.top().first) && (_index == other._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) return 0;
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) return true;
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
    : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const btree_reverse_iterator& it) noexcept
    : _index(it._index)
{
    auto temp_stack = it._path;
    std::vector<std::pair<btree_node* const*, size_t>> elems;
    while (!temp_stack.empty()) {
        auto [node_ptr, idx] = temp_stack.top();
        temp_stack.pop();
        elems.push_back({reinterpret_cast<btree_node* const*>(node_ptr), idx});
    }
    for (auto it_elem = elems.rbegin(); it_elem != elems.rend(); ++it_elem) {
        _path.push(*it_elem);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_const_iterator() const noexcept
{
    return btree_const_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator*() const noexcept
{
    btree_node* curr = *_path.top().first;
    return reinterpret_cast<reference>(curr->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }

    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index;
        btree_node* const* next_child = &(curr->_pointers[_index]);
        while (*next_child) {
            size_t last_ptr_idx = (*next_child)->_pointers.empty() ? 0 : (*next_child)->_pointers.size() - 1;
            _path.push({next_child, last_ptr_idx});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[last_ptr_idx]);
        }
        _index = (*next_child)->_keys.size() - 1;
    }
    else {
        if (_index > 0) {
            _index--;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                if (child_idx > 0) {
                    _index = child_idx - 1;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++(int)
{
    self tmp = *this;
    ++(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }

    btree_node* curr = *_path.top().first;

    if (!curr->_pointers.empty()) {
        _path.top().second = _index + 1;
        btree_node* const* next_child = &(curr->_pointers[_index + 1]);
        while (*next_child) {
            _path.push({next_child, 0});
            if ((*next_child)->_pointers.empty()) break;
            next_child = &((*next_child)->_pointers[0]);
        }
        _index = 0;
    }
    else {
        if (_index + 1 < curr->_keys.size()) {
            _index++;
            _path.top().second = _index;
        }
        else {
            while (!_path.empty()) {
                _path.pop();
                if (_path.empty()) {
                    _index = 0;
                    break;
                }
                auto [parent_ptr, child_idx] = _path.top();
                btree_node* parent = *parent_ptr;
                if (child_idx < parent->_keys.size()) {
                    _index = child_idx;
                    _path.top().second = _index;
                    break;
                }
            }
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--(int)
{
    self tmp = *this;
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty()) return true;
    if (_path.empty() || other._path.empty()) return false;
    return (*_path.top().first == *other._path.top().first) && (_index == other._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) return 0;
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) return true;
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::index() const noexcept
{
    return _index;
}

// endregion iterators implementation

// region element access implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    auto it = find(key);
    if (it == end()) throw out_of_range("Key not found");
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    auto result = emplace(key, tvalue());
    return const_cast<tvalue&>(result.first->second);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    auto result = emplace(std::move(key), tvalue());
    return const_cast<tvalue&>(result.first->second);
}

// endregion element access implementation

// region iterator begins implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::begin()
{
    std::stack<std::pair<btree_node**, size_t>> path;
    if (!_root) {
        return end();
    }
    btree_node** curr = &_root;
    while (*curr) {
        if ((*curr)->_pointers.empty()) {
            path.push({curr, 0});
            break;
        }
        path.push({curr, 0});
        curr = &((*curr)->_pointers[0]);
    }
    return btree_iterator(path, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::end()
{
    return btree_iterator(std::stack<std::pair<btree_node**, size_t>>(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::begin() const
{
    std::stack<std::pair<btree_node* const*, size_t>> path;
    if (!_root) {
        return end();
    }
    btree_node* const* curr = &_root;
    while (*curr) {
        if ((*curr)->_pointers.empty()) {
            path.push({curr, 0});
            break;
        }
        path.push({curr, 0});
        curr = &((*curr)->_pointers[0]);
    }
    return btree_const_iterator(path, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::end() const
{
    return btree_const_iterator(std::stack<std::pair<btree_node* const*, size_t>>(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin()
{
    std::stack<std::pair<btree_node**, size_t>> path;
    if (!_root) {
        return rend();
    }
    btree_node** curr = &_root;
    while (*curr) {
        if ((*curr)->_pointers.empty()) {
            size_t last_key_idx = (*curr)->_keys.empty() ? 0 : (*curr)->_keys.size() - 1;
            path.push({curr, last_key_idx});
            return btree_reverse_iterator(path, last_key_idx);
        }
        size_t last_ptr_idx = (*curr)->_pointers.empty() ? 0 : (*curr)->_pointers.size() - 1;
        path.push({curr, last_ptr_idx});
        curr = &((*curr)->_pointers[last_ptr_idx]);
    }
    return rend();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend()
{
    return btree_reverse_iterator(std::stack<std::pair<btree_node**, size_t>>(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin() const
{
    std::stack<std::pair<btree_node* const*, size_t>> path;
    if (!_root) {
        return rend();
    }
    btree_node* const* curr = &_root;
    while (*curr) {
        if ((*curr)->_pointers.empty()) {
            size_t last_key_idx = (*curr)->_keys.empty() ? 0 : (*curr)->_keys.size() - 1;
            path.push({curr, last_key_idx});
            return btree_const_reverse_iterator(path, last_key_idx);
        }
        size_t last_ptr_idx = (*curr)->_pointers.empty() ? 0 : (*curr)->_pointers.size() - 1;
        path.push({curr, last_ptr_idx});
        curr = &((*curr)->_pointers[last_ptr_idx]);
    }
    return rend();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend() const
{
    return btree_const_reverse_iterator(std::stack<std::pair<btree_node* const*, size_t>>(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crbegin() const
{
    return rbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crend() const
{
    return rend();
}

// endregion iterator begins implementation

// region lookup implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::size() const noexcept { return _size; }

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::empty() const noexcept { return _size == 0; }

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator 
B_tree<tkey, tvalue, compare, t>::find(const tkey& key) {
    btree_node** curr = &_root;
    std::stack<std::pair<btree_node**, size_t>> path;

    while (*curr) {
        size_t i = 0;
        while (i < (*curr)->_keys.size() && compare_keys((*curr)->_keys[i].first, key)) {
            i++;
        }
        if (i < (*curr)->_keys.size() && !compare_keys(key, (*curr)->_keys[i].first)) {
            path.push({curr, i});
            return btree_iterator(path, i);
        }
        if ((*curr)->_pointers.empty()) break;
        path.push({curr, i});
        curr = &((*curr)->_pointers[i]);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator 
B_tree<tkey, tvalue, compare, t>::find(const tkey& key) const {
    btree_node* const* curr = &_root;
    std::stack<std::pair<btree_node* const*, size_t>> path;

    while (*curr) {
        size_t i = 0;
        while (i < (*curr)->_keys.size() && compare_keys((*curr)->_keys[i].first, key)) {
            i++;
        }
        if (i < (*curr)->_keys.size() && !compare_keys(key, (*curr)->_keys[i].first)) {
            path.push({curr, i});
            return btree_const_iterator(path, i);
        }
        if ((*curr)->_pointers.empty()) break;
        path.push({curr, i});
        curr = &((*curr)->_pointers[i]);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key) {
    auto it = find(key);
    if (it == end()) throw out_of_range("key not found");
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) {
    btree_node** curr = &_root;
    std::stack<std::pair<btree_node**, size_t>> path;

    std::stack<std::pair<btree_node**, size_t>> best_path;
    size_t best_index = 0;
    bool found_any = false;

    while (*curr) {
        size_t i = 0;
        while (i < (*curr)->_keys.size() && compare_keys((*curr)->_keys[i].first, key)) {
            i++;
        }

        if (i < (*curr)->_keys.size()) {
            path.push({curr, i});
            best_path = path;
            best_index = i;
            found_any = true;

            if ((*curr)->_pointers.empty()) break;
            curr = &((*curr)->_pointers[i]);
        }
        else {
            if ((*curr)->_pointers.empty()) break;
            path.push({curr, i});
            curr = &((*curr)->_pointers[i]);
        }
    }

    return found_any ? btree_iterator(best_path, best_index) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator 
B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const {
    btree_node* const* curr = &_root;
    std::stack<std::pair<btree_node* const*, size_t>> path;

    std::stack<std::pair<btree_node* const*, size_t>> best_path;
    size_t best_index = 0;
    bool found_any = false;

    while (*curr) {
        size_t i = 0;
        while (i < (*curr)->_keys.size() && compare_keys((*curr)->_keys[i].first, key)) {
            i++;
        }

        if (i < (*curr)->_keys.size()) {
            path.push({curr, i});
            best_path = path;
            best_index = i;
            found_any = true;

            if ((*curr)->_pointers.empty()) break;
            curr = &((*curr)->_pointers[i]);
        }else {
            if ((*curr)->_pointers.empty()) break;
            path.push({curr, i});
            curr = &((*curr)->_pointers[i]);
        }
    }

    return found_any ? btree_const_iterator(best_path, best_index) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) {
    btree_node** curr = &_root;
    std::stack<std::pair<btree_node**, size_t>> path;

    std::stack<std::pair<btree_node**, size_t>> best_path;
    size_t best_index = 0;
    bool found_any = false;

    while (*curr) {
        size_t i = 0;
        while (i < (*curr)->_keys.size() && !compare_keys(key, (*curr)->_keys[i].first)) {
            i++;
        }

        if (i < (*curr)->_keys.size()) {
            path.push({curr, i});
            best_path = path;
            best_index = i;
            found_any = true;

            if ((*curr)->_pointers.empty()) break;
            curr = &((*curr)->_pointers[i]);
        }
        else {
            if ((*curr)->_pointers.empty()) break;
            path.push({curr, i});
            curr = &((*curr)->_pointers[i]);
        }
    }

    return found_any ? btree_iterator(best_path, best_index) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const {
    btree_node* const* curr = &_root;
    std::stack<std::pair<btree_node* const*, size_t>> path;

    std::stack<std::pair<btree_node* const*, size_t>> best_path;
    size_t best_index = 0;
    bool found_any = false;

    while (*curr) {
        size_t i = 0;
        while (i < (*curr)->_keys.size() && !compare_keys(key, (*curr)->_keys[i].first)) {
            i++;
        }

        if (i < (*curr)->_keys.size()) {
            path.push({curr, i});
            best_path = path;
            best_index = i;
            found_any = true;

            if ((*curr)->_pointers.empty()) break;
            curr = &((*curr)->_pointers[i]);
        }
        else {
            if ((*curr)->_pointers.empty()) break;
            path.push({curr, i});
            curr = &((*curr)->_pointers[i]);
        }
    }

    return found_any ? btree_const_iterator(best_path, best_index) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const {
    return find(key) != end();
}

// endregion lookup implementation

// region modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    clear_node(_root);
    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    return emplace(data.first, data.second);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    return emplace(std::move(data.first), std::move(data.second));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    const tkey& key = data.first;

    if (!_root) {
        _root = create_node();
        _root->_keys.push_back(std::move(data));
        _size = 1;
        return {find(key), true};
    }

    std::vector<std::pair<btree_node*, size_t>> path;
    btree_node* curr = _root;
    while (true) {
        size_t i = 0;
        while (i < curr->_keys.size() && compare_keys(curr->_keys[i].first, key)) {
            i++;
        }
        if (i < curr->_keys.size() && !compare_keys(key, curr->_keys[i].first)) {
            return {find(key), false};
        }
        if (curr->_pointers.empty()) {
            break;
        }
        path.push_back({curr, i});
        curr = curr->_pointers[i];
    }

    size_t insert_pos = 0;
    while (insert_pos < curr->_keys.size() && compare_keys(curr->_keys[insert_pos].first, key)) {
        insert_pos++;
    }
    curr->_keys.insert(curr->_keys.begin() + insert_pos, std::move(data));
    _size++;

    while (curr->_keys.size() > maximum_keys_in_node) {
        size_t median_idx = curr->_keys.size() / 2;
        tree_data_type median_key = std::move(curr->_keys[median_idx]);

        btree_node* right_sibling = create_node();

        right_sibling->_keys.insert(right_sibling->_keys.end(),
                                    std::make_move_iterator(curr->_keys.begin() + median_idx + 1),
                                    std::make_move_iterator(curr->_keys.end()));

        if (!curr->_pointers.empty()) {
            right_sibling->_pointers.insert(right_sibling->_pointers.end(),
                                            curr->_pointers.begin() + median_idx + 1,
                                            curr->_pointers.end());
            curr->_pointers.erase(curr->_pointers.begin() + median_idx + 1, curr->_pointers.end());
        }

        curr->_keys.erase(curr->_keys.begin() + median_idx, curr->_keys.end());

        if (path.empty()) {
            btree_node* new_root = create_node();
            new_root->_keys.push_back(std::move(median_key));
            new_root->_pointers.push_back(curr);
            new_root->_pointers.push_back(right_sibling);
            _root = new_root;
            break;
        }
        else {
            auto [parent, child_idx] = path.back();
            path.pop_back();

            parent->_keys.insert(parent->_keys.begin() + child_idx, std::move(median_key));
            parent->_pointers.insert(parent->_pointers.begin() + child_idx + 1, right_sibling);

            curr = parent;
        }
    }

    return {find(key), true};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    auto res = insert(data);
    if (!res.second) {
        const_cast<tvalue&>(res.first->second) = data.second;
    }
    return res.first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    auto res = insert(std::move(data));
    if (!res.second) {
        const_cast<tvalue&>(res.first->second) = std::move(data.second);
    }
    return res.first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    return insert_or_assign(std::move(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator pos)
{
    if (pos == end()) {
        return end();
    }
    auto next_it = pos;
    ++next_it;
    tkey next_key;
    bool has_next = (next_it != end());
    if (has_next) {
        next_key = next_it->first;
    }
    erase(pos->first);
    return has_next ? find(next_key) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator pos)
{
    if (pos == cend()) {
        return end();
    }
    auto next_it = pos;
    ++next_it;
    tkey next_key;
    bool has_next = (next_it != cend());
    if (has_next) {
        next_key = next_it->first;
    }
    erase(pos->first);
    return has_next ? find(next_key) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator beg, btree_iterator en)
{
    btree_iterator result = end();
    while (beg != en) {
        auto next_beg = beg;
        ++next_beg;
        result = erase(beg);
        beg = next_beg;
    }
    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator beg, btree_const_iterator en)
{
    btree_iterator result = end();
    while (beg != en) {
        auto next_beg = beg;
        ++next_beg;
        result = erase(beg);
        beg = next_beg;
    }
    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    auto pos = find(key);
    if (pos == end()) return end();

    auto next_it = pos;
    ++next_it;
    tkey next_key;
    bool has_next = (next_it != end());
    if (has_next) {
        next_key = next_it->first;
    }

    std::vector<std::pair<btree_node*, size_t>> path;
    btree_node* curr = _root;
    size_t found_idx = 0;
    bool found = false;
    btree_node* found_node = nullptr;

    while (curr) {
        size_t i = 0;
        while (i < curr->_keys.size() && compare_keys(curr->_keys[i].first, key)) {
            i++;
        }
        if (i < curr->_keys.size() && !compare_keys(key, curr->_keys[i].first)) {
            found_idx = i;
            found = true;
            found_node = curr;
            break;
        }
        if (curr->_pointers.empty()) break;
        path.push_back({curr, i});
        curr = curr->_pointers[i];
    }

    if (!found) {
        return end();
    }

    if (!found_node->_pointers.empty()) {
        path.push_back({found_node, found_idx});

        btree_node* pred_node = found_node->_pointers[found_idx];
        while (!pred_node->_pointers.empty()) {
            size_t last_idx = pred_node->_pointers.size() - 1;
            path.push_back({pred_node, last_idx});
            pred_node = pred_node->_pointers.back();
        }
        std::swap(found_node->_keys[found_idx], pred_node->_keys.back());
        curr = pred_node;
    }

    size_t erase_idx = 0;
    while (erase_idx < curr->_keys.size() && compare_keys(curr->_keys[erase_idx].first, key)) {
        erase_idx++;
    }
    curr->_keys.erase(curr->_keys.begin() + erase_idx);
    _size--;

    while (curr != _root && curr->_keys.size() < minimum_keys_in_node) {
        auto [parent, child_idx] = path.back();
        path.pop_back();

        btree_node* left_sib = (child_idx > 0) ? parent->_pointers[child_idx - 1] : nullptr;
        btree_node* right_sib = (child_idx < parent->_pointers.size() - 1) ? parent->_pointers[child_idx + 1] : nullptr;

        if (left_sib && left_sib->_keys.size() > minimum_keys_in_node) {
            curr->_keys.insert(curr->_keys.begin(), std::move(parent->_keys[child_idx - 1]));
            parent->_keys[child_idx - 1] = std::move(left_sib->_keys.back());
            left_sib->_keys.pop_back();

            if (!left_sib->_pointers.empty()) {
                curr->_pointers.insert(curr->_pointers.begin(), left_sib->_pointers.back());
                left_sib->_pointers.pop_back();
            }
            break;
        }
        else if (right_sib && right_sib->_keys.size() > minimum_keys_in_node) {
            curr->_keys.push_back(std::move(parent->_keys[child_idx]));
            parent->_keys[child_idx] = std::move(right_sib->_keys.front());
            right_sib->_keys.erase(right_sib->_keys.begin());

            if (!right_sib->_pointers.empty()) {
                curr->_pointers.push_back(right_sib->_pointers.front());
                right_sib->_pointers.erase(right_sib->_pointers.begin());
            }
            break;
        }
        else if (left_sib) {
            left_sib->_keys.push_back(std::move(parent->_keys[child_idx - 1]));
            left_sib->_keys.insert(left_sib->_keys.end(),
                                   std::make_move_iterator(curr->_keys.begin()),
                                   std::make_move_iterator(curr->_keys.end()));
            if (!curr->_pointers.empty()) {
                left_sib->_pointers.insert(left_sib->_pointers.end(), curr->_pointers.begin(), curr->_pointers.end());
            }

            parent->_keys.erase(parent->_keys.begin() + (child_idx - 1));
            parent->_pointers.erase(parent->_pointers.begin() + child_idx);

            destroy_node(curr);
            curr = parent;
        }
        else if (right_sib) {
            curr->_keys.push_back(std::move(parent->_keys[child_idx]));
            curr->_keys.insert(curr->_keys.end(),
                               std::make_move_iterator(right_sib->_keys.begin()),
                               std::make_move_iterator(right_sib->_keys.end()));
            if (!right_sib->_pointers.empty()) {
                curr->_pointers.insert(curr->_pointers.end(), right_sib->_pointers.begin(), right_sib->_pointers.end());
            }

            parent->_keys.erase(parent->_keys.begin() + child_idx);
            parent->_pointers.erase(parent->_pointers.begin() + (child_idx + 1));

            destroy_node(right_sib);
            curr = parent;
        }
    }

    if (_root && _root->_keys.empty()) {
        btree_node* old_root = _root;
        if (!_root->_pointers.empty()) {
            _root = _root->_pointers[0];
        }
        else {
            _root = nullptr;
        }
        destroy_node(old_root);
    }

    return has_next ? find(next_key) : end();
}

// endregion modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_node*
B_tree<tkey, tvalue, compare, t>::clone_node(btree_node* other_node)
{
    if (other_node == nullptr)
    {
        return nullptr;
    }

    btree_node* new_node = create_node();

    try
    {
        new_node->_keys = other_node->_keys;

        for (btree_node* child : other_node->_pointers)
        {
            new_node->_pointers.push_back(clone_node(child));
        }
    }
    catch (...)
    {
        clear_node(new_node);
        throw;
    }

    return new_node;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear_node(btree_node* node) noexcept
{
    if (node == nullptr)
    {
        return;
    }

    for (btree_node* child : node->_pointers)
    {
        clear_node(child);
    }

    destroy_node(node);
}

#endif
