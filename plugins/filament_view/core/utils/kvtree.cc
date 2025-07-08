#include "core/utils/kvtree.h"

namespace plugin_filament_view {

/*
 * KVTree
 */

template<typename Key, typename Value> KVTree<Key, Value>::~KVTree() {
  // TODO
}

template<typename Key, typename Value>
KVTreeNode<Key, Value>* KVTree<Key, Value>::get(const Key& key) const {
  auto it = nodeMap.find(key);
  if (it != nodeMap.end()) {
    return it->second;
  }
  return nullptr;
}

template<typename Key, typename Value> Value* KVTree<Key, Value>::getValue(const Key& key) const {
  KVTreeNode<Key, Value>* node = get(key);
  if (!node) {
    throw std::runtime_error("Key not found in tree");
  }
  return node->getValue();
}

template<typename Key, typename Value>
void KVTree<Key, Value>::insert(const Key& key, const Value& value, const Key* parentKey) {
  // Ensure the key does not already exist.
  if (nodeMap.find(key) != nodeMap.end()) {
    throw std::runtime_error("Key already exists in tree");
  }

  // Insert key-value into the tree’s value map.
  keyValueMap[key] = value;

  // Create a new node for this key.
  auto* newNode = new KVTreeNode<Key, Value>(key, this);
  nodeMap[key] = newNode;

  // If a parent key is provided, add the new node as a child.
  if (parentKey != nullptr) {
    auto* parent = get(*parentKey);
    // Exception: parent key not found.
    if (!parent) {
      throw std::runtime_error("Parent key not found");
    }

    parent->addChild(newNode);
    newNode->parent = parent;
  }
}

template<typename Key, typename Value>
void KVTree<Key, Value>::reparent(const Key& key, const Key* parentKey) {
  auto* node = get(key);
  // Exception: key not found.
  if (!node) {
    throw std::runtime_error("Key not found");
  }

  auto* parent = parentKey ? get(const_cast<Key&>(key))
                           : nullptr;  // nullptr if parent not found or is root.
  // Exception: parent key not found.
  if (parentKey && !parent) {
    throw std::runtime_error("Parent key not found");
  }

  // Reparent.
  if (node->parent) {
    node->parent->removeChild(const_cast<Key&>(key));
  }
  if (parent) {
    parent->addChild(node);
  }
  node->parent = parent;
}

template<typename Key, typename Value> void KVTree<Key, Value>::remove(const Key& key) {
  auto* node = get(key);

  // Exception: key not found.
  if (!node) {
    throw std::runtime_error("Key not found");
  }

  // Remove the node and its value.
  nodeMap.erase(key);
  keyValueMap.erase(key);

  // Remove the node from its parent’s children list.
  if (node->parent) {
    node->parent->removeChild(key);
  }

  // Remove the children of the node.
  // (this is done last for tail recursion)
  for (auto* child : node->getChildren()) {
    remove(const_cast<Key&>(child->getKey()));
  }
}

template<typename Key, typename Value> void KVTree<Key, Value>::clear() {
  // Clear the value map.
  keyValueMap.clear();

  // Delete all nodes.
  for (auto& [key, node] : nodeMap) {
    delete node;
  }
  nodeMap.clear();
}

/*
 * KVTreeNode
 */

template<typename Key, typename Value> Value* KVTreeNode<Key, Value>::getValue() const {
  auto it = treePtr->keyValueMap.find(key);
  if (it != treePtr->keyValueMap.end()) {
    return &(it->second);
  }
  throw std::runtime_error("Key not found in value map");
}

template<typename Key, typename Value> const Key& KVTreeNode<Key, Value>::getKey() const {
  return key;
}

template<typename Key, typename Value>
KVTreeNode<Key, Value>* KVTreeNode<Key, Value>::getParent() const {
  return parent;
}

template<typename Key, typename Value>
const std::vector<KVTreeNode<Key, Value>*>& KVTreeNode<Key, Value>::getChildren() const {
  // Copy the children vector to prevent modification.
  auto* childrenCopy = new std::vector<KVTreeNode<Key, Value>*>(children);
  return *childrenCopy;
}

template<typename Key, typename Value> void KVTreeNode<Key, Value>::addChild(KVTreeNode* child) {
  children.push_back(child);
}

template<typename Key, typename Value> void KVTreeNode<Key, Value>::removeChild(const Key& key) {
  children.erase(
    std::remove_if(
      children.begin(), children.end(), [&](auto& item) { return item->getKey() == key; }
    ),
    children.end()
  );
}

}  // namespace plugin_filament_view
