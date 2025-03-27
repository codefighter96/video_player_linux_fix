#pragma once

#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace plugin_filament_view {

// Forward declaration of KVTree so that KVTreeNode can refer to it.
template <typename Key, typename Value>
class KVTree;

template <typename Key, typename Value>
class KVTreeNode {
 public:
  // Retrieves the node’s associated value by looking up the key in the tree’s
  // map.
  Value* getValue() const;

  // Retrieves the node’s key.
  const Key& getKey() const;

  // Retrieves the node’s parent.
  KVTreeNode* getParent() const;

  // Retrieves the node’s children.
  // The returned vector is a copy and can be modified without affecting the
  // node.
  const std::vector<KVTreeNode*>& getChildren() const;

 private:
  Key key;
  KVTreeNode* parent;
  std::vector<KVTreeNode*> children;
  KVTree<Key, Value>* treePtr;

  // Constructor: only KVTree can create nodes.
  KVTreeNode(const Key& key,
             KVTree<Key, Value>* tree,
             KVTreeNode* parent = nullptr)
      : key(key), parent(parent), treePtr(tree) {}

  // Add a child to this node.
  void addChild(KVTreeNode* child);

  // Remove a child from this node.
  // It does not delete
  void removeChild(Key& key);

  // Grant KVTree access to all members of KVTreeNode.
  friend class KVTree<Key, Value>;

  // Disable copy construction/assignment.
  KVTreeNode(const KVTreeNode&) = delete;
  KVTreeNode& operator=(const KVTreeNode&) = delete;
};

template <typename Key, typename Value>
class KVTree {
 public:
  KVTree() = default;
  ~KVTree();

  friend class KVTreeNode<Key, Value>;

  // Retrieves the node associated with the given key.
  // Returns a pointer to the node, or nullptr if not found.
  KVTreeNode<Key, Value>* get(const Key& key) const;

  // Convenience method that returns the value associated with the node
  // identified by key.
  Value* getValue(const Key& key) const;

  // Inserts a new node with the given key and value.
  // If a parent key is provided (non-null), the new node is added as a child of
  // that parent. If the parent key is not found, the new node is added as a
  // root node (multiple roots are allowed).
  void insert(const Key& key,
              const Value& value,
              const Key* parentKey = nullptr);

  // Reparents a node identified by key to a new parent identified by parentKey.
  // If parentKey is null, the node becomes a root node.
  void reparent(const Key& key, const Key* parentKey = nullptr);

  // Removes the node identified by the given key, all its children and
  // associated values.
  void remove(Key& key);

  // Clears the tree of all nodes and values.
  void clear();

 private:
  // Stores key-value associations.
  std::unordered_map<Key, Value> keyValueMap;

  // Stores key-node associations for O(1) node retrieval.
  std::unordered_map<Key, KVTreeNode<Key, Value>*> nodeMap;
};

}  // namespace plugin_filament_view