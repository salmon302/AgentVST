/**
 * DSPRouter.h
 *
 * Builds and executes a DSP routing graph from schema routes.
 * Validation detects unknown nodes, cycles, and disconnected graphs.
 */
#pragma once

#include "DSPNode.h"
#include "SchemaParser.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <atomic>
#include <unordered_map>
#include <vector>

namespace AgentVST {

class ParameterCache;

class DSPRouter {
public:
    DSPRouter() = default;

    class RoutingError : public std::runtime_error {
    public:
        explicit RoutingError(const std::string& msg)
            : std::runtime_error(msg) {}
    };

    // Node registration
    void addNode(const std::string& nodeId, std::unique_ptr<DSPNode> node);
    void clearNodes();
    bool hasNode(const std::string& nodeId) const noexcept;

    // Graph setup and execution
    void setRouting(const std::vector<DSPRoute>& routes);
    void prepare(double sampleRate, int maxBlockSize);
    void bindParameters(const ParameterCache& cache);
    void reset();
    float processSample(int channel, float input) noexcept;

    // Parameter linking from schema parameter IDs to node parameters.
    void updateParameters(const ParameterCache& cache);

    bool isConfigured() const noexcept { return configured_; }
    std::size_t nodeCount() const noexcept { return nodes_.size(); }

private:
    static constexpr int kInputEndpoint  = -1;
    static constexpr int kOutputEndpoint = -2;

    struct Edge {
        int source = kInputEndpoint;
        int destination = kOutputEndpoint;
        std::unordered_map<std::string, std::string> parameterLinks;
    };

    struct ParameterBinding {
        std::string schemaParamId;
        int nodeIndex = -1;
        std::string nodeParameter;
        std::atomic<float>* cachedPtr = nullptr;
    };

    std::vector<std::unique_ptr<DSPNode>> nodes_;
    std::vector<std::string> nodeIds_;
    std::unordered_map<std::string, int> nodeIndexById_;

    std::vector<Edge> edges_;
    std::vector<int> executionOrder_;
    std::vector<std::vector<int>> nodeInputSources_;
    std::vector<int> outputSources_;
    std::vector<float> nodeValues_;
    std::vector<ParameterBinding> parameterBindings_;

    bool configured_ = false;

    int endpointToIndex(const std::string& endpoint, bool isSource) const;
    void buildExecutionPlan();
};

} // namespace AgentVST
