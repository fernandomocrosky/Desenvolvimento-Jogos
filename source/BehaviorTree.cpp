#include "BehaviorTree.hpp"

#include <stdexcept>

namespace {

/// Rejects null children up front, so tick() never has to check for them.
void requireChildren(const std::vector<BTNodePtr>& children, const char* who) {
    for (const BTNodePtr& child : children) {
        if (!child) {
            throw std::invalid_argument(std::string(who) + ": child node must not be null");
        }
    }
}

} // namespace

// --- SelectorNode -----------------------------------------------------------

SelectorNode::SelectorNode(std::vector<BTNodePtr> children)
    : m_children(std::move(children)) {
    requireChildren(m_children, "SelectorNode");
}

BTResult SelectorNode::tick(const BTContext& context) const {
    // Left to right; the first succeeding child decides, with its command.
    for (const BTNodePtr& child : m_children) {
        BTResult result = child->tick(context);
        if (result.success) {
            return result;
        }
    }
    return BTResult{ false, {} };   // all failed (or no children)
}

// --- SequenceNode -----------------------------------------------------------

SequenceNode::SequenceNode(std::vector<BTNodePtr> children)
    : m_children(std::move(children)) {
    requireChildren(m_children, "SequenceNode");
}

BTResult SequenceNode::tick(const BTContext& context) const {
    // Left to right; stops at the first failing child. On success it carries
    // the last non-empty command, so a trailing Condition does not erase it.
    std::string command;
    for (const BTNodePtr& child : m_children) {
        BTResult result = child->tick(context);
        if (!result.success) {
            return BTResult{ false, {} };
        }
        if (!result.command.empty()) {
            command = std::move(result.command);
        }
    }
    return BTResult{ true, std::move(command) };   // all succeeded (or no children)
}

// --- ConditionNode ----------------------------------------------------------

ConditionNode::ConditionNode(std::string name)
    : m_name(std::move(name)) {}

BTResult ConditionNode::tick(const BTContext& context) const {
    // Plain lookup: no name has a special meaning. A missing name is false.
    for (const auto& [name, value] : context) {
        if (name == m_name && value != 0) {
            return BTResult{ true, {} };
        }
    }
    return BTResult{ false, {} };
}

// --- ActionNode -------------------------------------------------------------

ActionNode::ActionNode(std::string command)
    : m_command(std::move(command)) {}

BTResult ActionNode::tick(const BTContext& /*context*/) const {
    return BTResult{ true, m_command };
}

// --- BehaviorTree -----------------------------------------------------------

BehaviorTree::BehaviorTree(BTNodePtr root)
    : m_root(std::move(root)) {
    if (!m_root) {
        throw std::invalid_argument("BehaviorTree: root node must not be null");
    }
}

std::string BehaviorTree::tick(const BTContext& context) const {
    // Re-evaluated from the root every tick; nothing is remembered.
    BTResult result = m_root->tick(context);
    return result.success ? std::move(result.command) : std::string{};
}
