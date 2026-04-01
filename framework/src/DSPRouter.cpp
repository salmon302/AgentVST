#include "DSPRouter.h"
#include "ParameterCache.h"

#include <algorithm>
#include <queue>

namespace AgentVST {

void DSPRouter::addNode(const std::string& nodeId, std::unique_ptr<DSPNode> node) {
    if (nodeId.empty())
        throw RoutingError("DSPRouter: node id cannot be empty");
    if (!node)
        throw RoutingError("DSPRouter: cannot add null node for id '" + nodeId + "'");
    if (nodeIndexById_.count(nodeId) != 0)
        throw RoutingError("DSPRouter: duplicate node id '" + nodeId + "'");

    const int idx = static_cast<int>(nodes_.size());
    node->setNodeId(nodeId);
    nodes_.push_back(std::move(node));
    nodeIds_.push_back(nodeId);
    nodeIndexById_[nodeId] = idx;
    configured_ = false;
}

void DSPRouter::clearNodes() {
    nodes_.clear();
    nodeIds_.clear();
    nodeIndexById_.clear();
    edges_.clear();
    executionOrder_.clear();
    nodeInputSources_.clear();
    outputSources_.clear();
    nodeValues_.clear();
    parameterBindings_.clear();
    configured_ = false;
}

bool DSPRouter::hasNode(const std::string& nodeId) const noexcept {
    return nodeIndexById_.count(nodeId) != 0;
}

int DSPRouter::endpointToIndex(const std::string& endpoint, bool isSource) const {
    if (endpoint == "input") {
        if (!isSource)
            throw RoutingError("DSPRouter: 'input' cannot be a destination");
        return kInputEndpoint;
    }

    if (endpoint == "output") {
        if (isSource)
            throw RoutingError("DSPRouter: 'output' cannot be a source");
        return kOutputEndpoint;
    }

    auto it = nodeIndexById_.find(endpoint);
    if (it == nodeIndexById_.end())
        throw RoutingError("DSPRouter: unknown node endpoint '" + endpoint + "'");
    return it->second;
}

void DSPRouter::setRouting(const std::vector<DSPRoute>& routes) {
    if (nodes_.empty())
        throw RoutingError("DSPRouter: setRouting() requires at least one registered node");
    if (routes.empty())
        throw RoutingError("DSPRouter: routing list cannot be empty");

    edges_.clear();
    edges_.reserve(routes.size());

    for (const auto& route : routes) {
        Edge edge;
        edge.source = endpointToIndex(route.source, true);
        edge.destination = endpointToIndex(route.destination, false);
        edge.parameterLinks = route.parameterLinks;

        if (edge.source == edge.destination)
            throw RoutingError("DSPRouter: self-edge is not allowed ('" + route.source + "')");

        edges_.push_back(std::move(edge));
    }

    buildExecutionPlan();
    configured_ = true;
}

void DSPRouter::buildExecutionPlan() {
    const int n = static_cast<int>(nodes_.size());

    std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
    std::vector<int> indegree(static_cast<std::size_t>(n), 0);
    std::vector<bool> reachableFromInput(static_cast<std::size_t>(n), false);
    std::vector<bool> canReachOutput(static_cast<std::size_t>(n), false);

    nodeInputSources_.assign(static_cast<std::size_t>(n), {});
    outputSources_.clear();
    parameterBindings_.clear();

    std::vector<std::vector<int>> reverseAdj(static_cast<std::size_t>(n));

    for (const auto& edge : edges_) {
        if (edge.destination >= 0) {
            nodeInputSources_[static_cast<std::size_t>(edge.destination)].push_back(edge.source);
        } else {
            outputSources_.push_back(edge.source);
        }

        if (edge.source >= 0 && edge.destination >= 0) {
            adj[static_cast<std::size_t>(edge.source)].push_back(edge.destination);
            reverseAdj[static_cast<std::size_t>(edge.destination)].push_back(edge.source);
            ++indegree[static_cast<std::size_t>(edge.destination)];
        }

        if (edge.source == kInputEndpoint && edge.destination >= 0)
            reachableFromInput[static_cast<std::size_t>(edge.destination)] = true;
        if (edge.destination == kOutputEndpoint && edge.source >= 0)
            canReachOutput[static_cast<std::size_t>(edge.source)] = true;

        for (const auto& [schemaParam, targetExpr] : edge.parameterLinks) {
            std::string targetNodeId;
            std::string targetParam;

            const auto dotPos = targetExpr.find('.');
            if (dotPos == std::string::npos) {
                if (edge.destination < 0) {
                    throw RoutingError(
                        "DSPRouter: parameter link '" + schemaParam + "' must specify node.param when destination is output");
                }
                targetNodeId = nodeIds_[static_cast<std::size_t>(edge.destination)];
                targetParam = targetExpr;
            } else {
                targetNodeId = targetExpr.substr(0, dotPos);
                targetParam = targetExpr.substr(dotPos + 1);
            }

            auto it = nodeIndexById_.find(targetNodeId);
            if (it == nodeIndexById_.end())
                throw RoutingError("DSPRouter: parameter link references unknown node '" + targetNodeId + "'");
            if (targetParam.empty())
                throw RoutingError("DSPRouter: parameter link target parameter cannot be empty");

            parameterBindings_.push_back({schemaParam, it->second, targetParam});
        }
    }

    // Topological sort for node subgraph.
    std::queue<int> q;
    for (int i = 0; i < n; ++i) {
        if (indegree[static_cast<std::size_t>(i)] == 0)
            q.push(i);
    }

    executionOrder_.clear();
    executionOrder_.reserve(static_cast<std::size_t>(n));

    while (!q.empty()) {
        const int u = q.front();
        q.pop();
        executionOrder_.push_back(u);

        for (int v : adj[static_cast<std::size_t>(u)]) {
            auto& deg = indegree[static_cast<std::size_t>(v)];
            --deg;
            if (deg == 0)
                q.push(v);
        }
    }

    if (static_cast<int>(executionOrder_.size()) != n)
        throw RoutingError("DSPRouter: cycle detected in routing graph");

    // Reachability from input.
    std::queue<int> fromInput;
    for (int i = 0; i < n; ++i) {
        if (reachableFromInput[static_cast<std::size_t>(i)])
            fromInput.push(i);
    }
    while (!fromInput.empty()) {
        const int u = fromInput.front();
        fromInput.pop();
        for (int v : adj[static_cast<std::size_t>(u)]) {
            if (!reachableFromInput[static_cast<std::size_t>(v)]) {
                reachableFromInput[static_cast<std::size_t>(v)] = true;
                fromInput.push(v);
            }
        }
    }

    // Reachability to output (reverse graph).
    std::queue<int> toOutput;
    for (int i = 0; i < n; ++i) {
        if (canReachOutput[static_cast<std::size_t>(i)])
            toOutput.push(i);
    }
    while (!toOutput.empty()) {
        const int u = toOutput.front();
        toOutput.pop();
        for (int v : reverseAdj[static_cast<std::size_t>(u)]) {
            if (!canReachOutput[static_cast<std::size_t>(v)]) {
                canReachOutput[static_cast<std::size_t>(v)] = true;
                toOutput.push(v);
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        if (!reachableFromInput[static_cast<std::size_t>(i)])
            throw RoutingError("DSPRouter: node '" + nodeIds_[static_cast<std::size_t>(i)] + "' is disconnected from input");
        if (!canReachOutput[static_cast<std::size_t>(i)])
            throw RoutingError("DSPRouter: node '" + nodeIds_[static_cast<std::size_t>(i)] + "' cannot reach output");
    }

    if (outputSources_.empty())
        throw RoutingError("DSPRouter: no route reaches output");

    nodeValues_.assign(static_cast<std::size_t>(n), 0.0f);
}

void DSPRouter::prepare(double sampleRate, int maxBlockSize) {
    for (auto& node : nodes_)
        node->prepare(sampleRate, maxBlockSize);
}

void DSPRouter::bindParameters(const ParameterCache& cache) {
    for (auto& binding : parameterBindings_)
        binding.cachedPtr = cache.getPointer(binding.schemaParamId);
}

void DSPRouter::reset() {
    for (auto& node : nodes_)
        node->reset();
    std::fill(nodeValues_.begin(), nodeValues_.end(), 0.0f);
}

void DSPRouter::updateParameters(const ParameterCache& cache) {
    for (auto& binding : parameterBindings_) {
        if (binding.cachedPtr == nullptr)
            binding.cachedPtr = cache.getPointer(binding.schemaParamId);

        const float value = (binding.cachedPtr != nullptr)
            ? binding.cachedPtr->load(std::memory_order_relaxed)
            : 0.0f;

        nodes_[static_cast<std::size_t>(binding.nodeIndex)]->setParameter(binding.nodeParameter, value);
    }
}

float DSPRouter::processSample(int channel, float input) noexcept {
    if (!configured_)
        return input;

    for (int nodeIndex : executionOrder_) {
        float nodeInput = 0.0f;
        for (int source : nodeInputSources_[static_cast<std::size_t>(nodeIndex)]) {
            if (source == kInputEndpoint)
                nodeInput += input;
            else
                nodeInput += nodeValues_[static_cast<std::size_t>(source)];
        }

        nodeValues_[static_cast<std::size_t>(nodeIndex)] =
            nodes_[static_cast<std::size_t>(nodeIndex)]->processSample(channel, nodeInput);
    }

    float out = 0.0f;
    for (int source : outputSources_) {
        if (source == kInputEndpoint)
            out += input;
        else
            out += nodeValues_[static_cast<std::size_t>(source)];
    }
    return out;
}

} // namespace AgentVST
