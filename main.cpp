#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <queue>
#include <limits>
#include <numeric>
#include <utility>
#include <functional>

namespace Theme
{
    const sf::Color Background(8, 11, 20);
    const sf::Color Sidebar(14, 20, 34);
    const sf::Color Canvas(8, 11, 20);
    const sf::Color Node(53, 242, 181);
    const sf::Color Selected(167, 139, 250);
    const sf::Color Green(53, 242, 181);
    const sf::Color ActiveEdge(69, 200, 255);
    const sf::Color Edge(101, 113, 138);
    const sf::Color Text(0xF1, 0xF5, 0xF9);
    const sf::Color Grid(25, 34, 52);
}

struct Node
{
    std::string name;
    sf::Vector2f position;
};

struct Edge
{
    int from;
    int to;
    int weight;
};

// Tree data is stored separately from the graph's Node and Edge vectors.
struct TreeNode
{
    int value;
    int left = -1;
    int right = -1;
};

// Center text around a position.
void centerText(sf::Text& text, sf::Vector2f position)
{
    sf::FloatRect bounds = text.getLocalBounds();

    text.setOrigin({
        bounds.position.x + bounds.size.x / 2.f,
        bounds.position.y + bounds.size.y / 2.f
    });

    text.setPosition(position);
}

// Draw a rounded UI card using SFML primitives (keeps compatibility with SFML 3).
void drawRoundedPanel(sf::RenderWindow& window, sf::FloatRect bounds,
                      sf::Color fill, sf::Color outline, float radius = 12.f,
                      float outlineThickness = 1.4f)
{
    // Clean panel rendering: avoid full-circle corner outlines, which
    // appeared as detached oval marks around the UI in the previous version.
    (void)radius; // SFML 3 has no built-in rounded RectangleShape.
    sf::RectangleShape panel(bounds.size);
    panel.setPosition(bounds.position);
    panel.setFillColor(fill);
    panel.setOutlineColor(outline);
    panel.setOutlineThickness(outlineThickness);
    window.draw(panel);
}


// Draw a thick line as a quadrilateral instead of a one-pixel SFML line.
void drawThickLine(sf::RenderWindow& window, sf::Vector2f start,
                   sf::Vector2f finish, sf::Color color, float thickness)
{
    const float dx = finish.x - start.x;
    const float dy = finish.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);

    if (length < 0.01f || thickness <= 0.f)
        return;

    const float halfThickness = thickness / 2.f;
    const float perpendicularX = -dy / length * halfThickness;
    const float perpendicularY =  dx / length * halfThickness;

    sf::ConvexShape line(4);
    line.setPoint(0, {start.x + perpendicularX, start.y + perpendicularY});
    line.setPoint(1, {finish.x + perpendicularX, finish.y + perpendicularY});
    line.setPoint(2, {finish.x - perpendicularX, finish.y - perpendicularY});
    line.setPoint(3, {start.x - perpendicularX, start.y - perpendicularY});
    line.setFillColor(color);
    window.draw(line);
}

// Return the index of a node under the mouse, or -1 if none.
int findNodeAtPosition(
    const std::vector<Node>& nodes,
    sf::Vector2f mousePosition)
{
    for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; i--)
    {
        float dx = mousePosition.x - nodes[i].position.x;
        float dy = mousePosition.y - nodes[i].position.y;

        if (dx * dx + dy * dy <= 25.f * 25.f)
        {
            return i;
        }
    }

    return -1;
}

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({1450, 800}),
        "Project - Graph Visualizer"
    );

    window.setFramerateLimit(60);

    std::vector<Node> nodes;
    std::vector<Edge> edges;

    // Current interaction modes
    bool enteringNodeName = false;
    bool placingNode = false;
    bool selectingEdge = false;
    bool enteringWeight = false;
    bool deletingNode = false;

    // Text input
    std::string nodeNameInput;
    std::string weightInput;

    // Selected nodes for creating an edge
    int firstSelectedNode = -1;
    int secondSelectedNode = -1;

    // Node being dragged
    int draggingNode = -1;
    sf::Vector2f dragOffset{0.f, 0.f};

    // Algorithm visualization state
    std::vector<int> highlightedEdges;
    std::vector<int> highlightedNodes;
    std::string algorithmStatus = "Choose an algorithm below.";
    int dijkstraStart = -1;
    bool choosingDijkstraStart = false;
    bool choosingDijkstraEnd = false;

    // Separate Binary Search Tree mode and its state.
    bool treeMode = false;
    bool treeInputActive = false;
    std::string treeInput;
    std::vector<TreeNode> treeNodes;
    int treeRoot = -1;
    std::vector<int> treeHighlightedNodes;
    std::vector<int> treeAnimationOrder;
    std::size_t treeAnimationCursor = 0;
    bool treeAnimationRunning = false;
    sf::Clock treeAnimationClock;

    // Load Arial from Windows first, then try common macOS font locations.
    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf") &&
        !font.openFromFile("/System/Library/Fonts/Supplemental/Arial.ttf") &&
        !font.openFromFile("/System/Library/Fonts/SFNS.ttf"))
    {
        std::cerr << "Could not load Arial. Please install Arial or update the font path.\n";
        return 1;
    }

    // Button drawing helper
    auto drawButton = [&](const sf::RectangleShape& button,
                          const std::string& label)
    {
        sf::FloatRect bounds = button.getGlobalBounds();
        sf::Color buttonFill(12, 13, 26);
        sf::Color buttonOutline(47, 62, 91);
        drawRoundedPanel(window, bounds, buttonFill, buttonOutline, 9.f, 1.2f);

        // Keep button captions inside their rectangles, including long labels.
        unsigned int captionSize = 16;
        sf::Text text(font, label, captionSize);
        while (text.getLocalBounds().size.x > bounds.size.x - 12.f && captionSize > 10)
        {
            --captionSize;
            text.setCharacterSize(captionSize);
        }
        text.setFillColor(Theme::Text);
        centerText(
            text,
            {
                bounds.position.x + bounds.size.x / 2.f,
                bounds.position.y + bounds.size.y / 2.f
            }
        );

        window.draw(text);
    };

    // Buttons
    sf::RectangleShape addNodeButton({95.f, 46.f});
    addNodeButton.setPosition({295.f, 158.f});
    addNodeButton.setFillColor(Theme::Canvas); addNodeButton.setOutlineThickness(1.f); addNodeButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape placeNodeButton({95.f, 46.f});
    placeNodeButton.setPosition({397.f, 158.f});
    placeNodeButton.setFillColor(Theme::Canvas); placeNodeButton.setOutlineThickness(1.f); placeNodeButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape addEdgeButton({95.f, 46.f});
    addEdgeButton.setPosition({499.f, 158.f});
    addEdgeButton.setFillColor(Theme::Canvas); addEdgeButton.setOutlineThickness(1.f); addEdgeButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape createEdgeButton({145.f, 40.f});
    createEdgeButton.setPosition({295.f, 302.f});
    createEdgeButton.setFillColor(Theme::Canvas); createEdgeButton.setOutlineThickness(1.f); createEdgeButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape deleteNodeButton({145.f, 40.f});
    deleteNodeButton.setPosition({455.f, 302.f});
    deleteNodeButton.setFillColor(Theme::Canvas); deleteNodeButton.setOutlineThickness(1.f); deleteNodeButton.setOutlineColor(Theme::Edge);

    // Algorithm buttons
    sf::RectangleShape dijkstraButton({145.f, 58.f});
    dijkstraButton.setPosition({295.f, 408.f});
    dijkstraButton.setFillColor(Theme::Canvas); dijkstraButton.setOutlineThickness(1.f); dijkstraButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape primButton({145.f, 58.f});
    primButton.setPosition({455.f, 408.f});
    primButton.setFillColor(Theme::Canvas); primButton.setOutlineThickness(1.f); primButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape kruskalButton({145.f, 58.f});
    kruskalButton.setPosition({295.f, 474.f});
    kruskalButton.setFillColor(Theme::Canvas); kruskalButton.setOutlineThickness(1.f); kruskalButton.setOutlineColor(Theme::Edge);

    // Switch between graph algorithms and a separate BST playground.
    sf::RectangleShape treeModeButton({145.f, 58.f});
    treeModeButton.setPosition({455.f, 474.f});
    treeModeButton.setFillColor(Theme::Canvas); treeModeButton.setOutlineThickness(1.f); treeModeButton.setOutlineColor(Theme::Edge);

    sf::RectangleShape treeBackButton({145.f, 38.f});
    treeBackButton.setPosition({295.f, 485.f});
    treeBackButton.setFillColor(Theme::Canvas); treeBackButton.setOutlineThickness(1.f); treeBackButton.setOutlineColor(Theme::Edge);

    // Tree mode controls.
    sf::RectangleShape treeInputBox({305.f, 38.f});
    treeInputBox.setPosition({295.f, 174.f});
    treeInputBox.setFillColor(sf::Color(50, 50, 50));
    treeInputBox.setOutlineThickness(2.f);
    treeInputBox.setOutlineColor(sf::Color::White);

    sf::RectangleShape treeInsertButton({145.f, 38.f});
    treeInsertButton.setPosition({295.f, 225.f});
    treeInsertButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treeSearchButton({145.f, 38.f});
    treeSearchButton.setPosition({455.f, 225.f});
    treeSearchButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treeInorderButton({145.f, 38.f});
    treeInorderButton.setPosition({295.f, 275.f});
    treeInorderButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treePreorderButton({145.f, 38.f});
    treePreorderButton.setPosition({455.f, 275.f});
    treePreorderButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treePostorderButton({145.f, 38.f});
    treePostorderButton.setPosition({295.f, 325.f});
    treePostorderButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treeLevelButton({145.f, 38.f});
    treeLevelButton.setPosition({455.f, 325.f});
    treeLevelButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treeMetricsButton({145.f, 38.f});
    treeMetricsButton.setPosition({295.f, 375.f});
    treeMetricsButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treeClearButton({145.f, 38.f});
    treeClearButton.setPosition({455.f, 375.f});
    treeClearButton.setFillColor(Theme::Canvas);

    sf::RectangleShape treeDeleteButton({145.f, 38.f});
    treeDeleteButton.setPosition({455.f, 425.f});
    treeDeleteButton.setFillColor(Theme::Canvas);

    std::vector<int> animationEdgeOrder;
    std::vector<int> animationNodeOrder;
    std::size_t animationEdgeCursor = 0;
    int currentlyAnimatingEdge = -1;
    int animatingFromNode = -1;
    int animatingToNode = -1;
    float currentEdgeProgress = 0.f;
    sf::Clock animationClock;
    sf::Clock nodeRevealClock;
    bool animationRunning = false;
    bool nodeRevealPending = false;

    auto clearAlgorithmResult = [&]()
    {
        highlightedEdges.clear();
        highlightedNodes.clear();
        animationEdgeOrder.clear();
        animationNodeOrder.clear();
        animationEdgeCursor = 0;
        currentlyAnimatingEdge = -1;
        animatingFromNode = -1;
        animatingToNode = -1;
        currentEdgeProgress = 0.f;
        animationRunning = false;
        nodeRevealPending = false;
        dijkstraStart = -1;
        choosingDijkstraStart = false;
        choosingDijkstraEnd = false;
    };

    // Prepare one selected edge: make its starting endpoint green first,
    // then animate the green line toward its other endpoint.
    auto prepareCurrentAnimationEdge = [&]()
    {
        currentlyAnimatingEdge = animationEdgeOrder[animationEdgeCursor];

        const Edge& edge = edges[currentlyAnimatingEdge];

        bool fromAlreadyGreen =
            std::find(highlightedNodes.begin(), highlightedNodes.end(),
                      edge.from) != highlightedNodes.end();
        bool toAlreadyGreen =
            std::find(highlightedNodes.begin(), highlightedNodes.end(),
                      edge.to) != highlightedNodes.end();

        // Continue outward from a node already revealed by the previous
        // edge, when possible. Otherwise start from the edge's 'from' node.
        if (toAlreadyGreen && !fromAlreadyGreen)
        {
            animatingFromNode = edge.to;
            animatingToNode = edge.from;
        }
        else
        {
            animatingFromNode = edge.from;
            animatingToNode = edge.to;
        }

        if (std::find(highlightedNodes.begin(), highlightedNodes.end(),
                      animatingFromNode) == highlightedNodes.end())
        {
            highlightedNodes.push_back(animatingFromNode);
        }

        currentEdgeProgress = 0.f;
        animationRunning = true;
        nodeRevealPending = false;
        animationClock.restart();
    };

    // For every result edge, animate in this order:
    // first endpoint turns green -> green line grows -> wait 0.5 s ->
    // second endpoint turns green -> proceed to the next edge.
    auto beginResultAnimation = [&]()
    {
        animationEdgeOrder = highlightedEdges;
        animationNodeOrder = highlightedNodes;

        highlightedEdges.clear();
        highlightedNodes.clear();

        animationEdgeCursor = 0;
        currentlyAnimatingEdge = -1;
        animatingFromNode = -1;
        animatingToNode = -1;
        currentEdgeProgress = 0.f;
        animationRunning = false;
        nodeRevealPending = false;

        if (!animationEdgeOrder.empty())
        {
            prepareCurrentAnimationEdge();
        }
        else
        {
            nodeRevealPending = true;
            nodeRevealClock.restart();
            algorithmStatus += "  (No result edges; revealing result node(s).)";
        }
    };

    // Dijkstra: compute the shortest path from source to destination.
    auto runDijkstra = [&](int source, int destination)
    {
        clearAlgorithmResult();

        if (source < 0 || destination < 0 ||
            source >= static_cast<int>(nodes.size()) ||
            destination >= static_cast<int>(nodes.size()))
        {
            algorithmStatus = "Select valid start and destination nodes.";
            return;
        }

        const int n = static_cast<int>(nodes.size());
        const int INF = std::numeric_limits<int>::max();
        std::vector<int> distance(n, INF);
        std::vector<int> previousNode(n, -1);
        std::vector<int> previousEdge(n, -1);

        using QueueItem = std::pair<int, int>;
        std::priority_queue<QueueItem,
                            std::vector<QueueItem>,
                            std::greater<QueueItem>> pq;

        distance[source] = 0;
        pq.push({0, source});

        while (!pq.empty())
        {
            auto [currentDistance, currentNode] = pq.top();
            pq.pop();

            if (currentDistance != distance[currentNode])
                continue;

            for (int e = 0; e < static_cast<int>(edges.size()); ++e)
            {
                const Edge& edge = edges[e];
                int neighbor = -1;

                if (edge.from == currentNode)
                    neighbor = edge.to;
                else if (edge.to == currentNode)
                    neighbor = edge.from;

                if (neighbor == -1)
                    continue;

                // Edge weights are entered as non-negative numbers.
                if (currentDistance <= INF - edge.weight &&
                    currentDistance + edge.weight < distance[neighbor])
                {
                    distance[neighbor] = currentDistance + edge.weight;
                    previousNode[neighbor] = currentNode;
                    previousEdge[neighbor] = e;
                    pq.push({distance[neighbor], neighbor});
                }
            }
        }

        if (distance[destination] == INF)
        {
            algorithmStatus = "Dijkstra: no path between those nodes.";
            return;
        }

        int current = destination;
        highlightedNodes.push_back(current);

        while (current != source)
        {
            int edgeIndex = previousEdge[current];
            int parent = previousNode[current];

            if (edgeIndex == -1 || parent == -1)
                break;

            highlightedEdges.push_back(edgeIndex);
            current = parent;
            highlightedNodes.push_back(current);
        }

        // The predecessor walk above reconstructs the path from destination
        // back to source. Reverse the edge list so the animation travels
        // naturally from the selected source toward the destination.
        std::reverse(highlightedEdges.begin(), highlightedEdges.end());
        std::reverse(highlightedNodes.begin(), highlightedNodes.end());

        algorithmStatus = "DIJKSTRA ALGORITHM (SHORTEST PATH): " +
                          std::to_string(distance[destination]);
        beginResultAnimation();
    };

    // Prim: grow a minimum spanning tree from node 0.
    auto runPrim = [&]()
    {
        clearAlgorithmResult();

        const int n = static_cast<int>(nodes.size());
        if (n == 0)
        {
            algorithmStatus = "Prim: add some nodes first.";
            return;
        }

        std::vector<bool> inTree(n, false);
        inTree[0] = true;
        highlightedNodes.push_back(0);
        int totalWeight = 0;

        while (static_cast<int>(highlightedNodes.size()) < n)
        {
            int bestEdge = -1;
            int bestWeight = std::numeric_limits<int>::max();
            int bestNextNode = -1;

            for (int e = 0; e < static_cast<int>(edges.size()); ++e)
            {
                const Edge& edge = edges[e];
                bool fromInside = inTree[edge.from];
                bool toInside = inTree[edge.to];

                if (fromInside == toInside)
                    continue;

                if (edge.weight < bestWeight)
                {
                    bestWeight = edge.weight;
                    bestEdge = e;
                    bestNextNode = fromInside ? edge.to : edge.from;
                }
            }

            if (bestEdge == -1)
            {
                clearAlgorithmResult();
                algorithmStatus = "Prim: graph is disconnected; no spanning tree.";
                return;
            }

            highlightedEdges.push_back(bestEdge);
            inTree[bestNextNode] = true;
            highlightedNodes.push_back(bestNextNode);
            totalWeight += bestWeight;
        }

        algorithmStatus = "PRIM ALGORITHM (MST): " +
                          std::to_string(totalWeight);
        beginResultAnimation();
    };

    // Kruskal: sort edges and accept them when they join different components.
    auto runKruskal = [&]()
    {
        clearAlgorithmResult();

        const int n = static_cast<int>(nodes.size());
        if (n == 0)
        {
            algorithmStatus = "Kruskal: add some nodes first.";
            return;
        }

        std::vector<int> parent(n);
        std::vector<int> rank(n, 0);
        std::iota(parent.begin(), parent.end(), 0);

        auto findRoot = [&](int x)
        {
            while (parent[x] != x)
            {
                parent[x] = parent[parent[x]];
                x = parent[x];
            }
            return x;
        };

        std::vector<int> sortedEdgeIndices(edges.size());
        std::iota(sortedEdgeIndices.begin(), sortedEdgeIndices.end(), 0);

        std::sort(sortedEdgeIndices.begin(), sortedEdgeIndices.end(),
                  [&](int a, int b)
                  {
                      return edges[a].weight < edges[b].weight;
                  });

        int totalWeight = 0;

        for (int edgeIndex : sortedEdgeIndices)
        {
            const Edge& edge = edges[edgeIndex];
            int rootA = findRoot(edge.from);
            int rootB = findRoot(edge.to);

            if (rootA == rootB)
                continue;

            if (rank[rootA] < rank[rootB])
                std::swap(rootA, rootB);

            parent[rootB] = rootA;
            if (rank[rootA] == rank[rootB])
                rank[rootA]++;

            highlightedEdges.push_back(edgeIndex);
            totalWeight += edge.weight;

            if (static_cast<int>(highlightedEdges.size()) == n - 1)
                break;
        }

        if (static_cast<int>(highlightedEdges.size()) != n - 1)
        {
            clearAlgorithmResult();
            algorithmStatus = "Kruskal: graph is disconnected; no spanning tree.";
            return;
        }

        for (int i = 0; i < n; ++i)
            highlightedNodes.push_back(i);

        algorithmStatus = "KRUSKAL ALGORITHM (MST): " +
                          std::to_string(totalWeight);
        beginResultAnimation();
    };

    // ---- Binary Search Tree operations (independent of graph data) ----
    auto clearTreeAnimation = [&]()
    {
        treeHighlightedNodes.clear();
        treeAnimationOrder.clear();
        treeAnimationCursor = 0;
        treeAnimationRunning = false;
    };

    auto startTreeNodeAnimation = [&](const std::vector<int>& order)
    {
        clearTreeAnimation();
        treeAnimationOrder = order;
        if (!treeAnimationOrder.empty())
        {
            treeAnimationRunning = true;
            treeAnimationClock.restart();
        }
    };

    auto insertTreeValue = [&](int value)
    {
        clearTreeAnimation();
        if (treeRoot == -1)
        {
            treeNodes.push_back({value, -1, -1});
            treeRoot = 0;
            algorithmStatus = "BST INSERT: inserted root " + std::to_string(value);
            return;
        }

        int current = treeRoot;
        while (true)
        {
            if (value == treeNodes[current].value)
            {
                algorithmStatus = "BST INSERT: " + std::to_string(value) + " already exists (duplicates ignored).";
                return;
            }

            if (value < treeNodes[current].value)
            {
                if (treeNodes[current].left == -1)
                {
                    int newIndex = static_cast<int>(treeNodes.size());
                    treeNodes.push_back({value, -1, -1});
                    treeNodes[current].left = newIndex;
                    algorithmStatus = "BST INSERT: inserted " + std::to_string(value);
                    return;
                }
                current = treeNodes[current].left;
            }
            else
            {
                if (treeNodes[current].right == -1)
                {
                    int newIndex = static_cast<int>(treeNodes.size());
                    treeNodes.push_back({value, -1, -1});
                    treeNodes[current].right = newIndex;
                    algorithmStatus = "BST INSERT: inserted " + std::to_string(value);
                    return;
                }
                current = treeNodes[current].right;
            }
        }
    };

    auto searchTreeValue = [&](int value)
    {
        clearTreeAnimation();
        std::vector<int> path;
        int current = treeRoot;
        while (current != -1)
        {
            path.push_back(current);
            if (treeNodes[current].value == value)
            {
                algorithmStatus = "BST SEARCH: " + std::to_string(value) + " FOUND";
                startTreeNodeAnimation(path);
                return;
            }
            current = (value < treeNodes[current].value)
                ? treeNodes[current].left : treeNodes[current].right;
        }

        algorithmStatus = "BST SEARCH: " + std::to_string(value) + " NOT FOUND";
        startTreeNodeAnimation(path);
    };

    // Delete a value from the BST while preserving the normal BST structure.
    // Nodes are stored by index, so removed slots can remain unused safely.
    auto deleteTreeValue = [&](int value)
    {
        clearTreeAnimation();
        bool found = false;

        std::function<int(int)> eraseNode = [&](int index) -> int
        {
            if (index == -1) return -1;

            if (value < treeNodes[index].value)
            {
                treeNodes[index].left = eraseNode(treeNodes[index].left);
                return index;
            }
            if (value > treeNodes[index].value)
            {
                treeNodes[index].right = eraseNode(treeNodes[index].right);
                return index;
            }

            found = true;
            // Case 1: no left child; replace this node with its right subtree.
            if (treeNodes[index].left == -1)
                return treeNodes[index].right;
            // Case 2: no right child; replace this node with its left subtree.
            if (treeNodes[index].right == -1)
                return treeNodes[index].left;

            // Case 3: two children; copy the inorder successor, then remove it.
            int successor = treeNodes[index].right;
            while (treeNodes[successor].left != -1)
                successor = treeNodes[successor].left;

            int successorValue = treeNodes[successor].value;
            treeNodes[index].value = successorValue;
            int oldValue = value;
            value = successorValue;
            treeNodes[index].right = eraseNode(treeNodes[index].right);
            value = oldValue;
            return index;
        };

        treeRoot = eraseNode(treeRoot);
        if (found)
            algorithmStatus = "BST DELETE: removed " + std::to_string(value);
        else
            algorithmStatus = "BST DELETE: " + std::to_string(value) + " NOT FOUND";
    };

    auto runTreeTraversal = [&](const std::string& traversalName)
    {
        clearTreeAnimation();
        std::vector<int> order;

        if (traversalName == "INORDER")
        {
            auto visit = [&](auto&& self, int index) -> void
            {
                if (index == -1) return;
                self(self, treeNodes[index].left);
                order.push_back(index);
                self(self, treeNodes[index].right);
            };
            visit(visit, treeRoot);
        }
        else if (traversalName == "PREORDER")
        {
            auto visit = [&](auto&& self, int index) -> void
            {
                if (index == -1) return;
                order.push_back(index);
                self(self, treeNodes[index].left);
                self(self, treeNodes[index].right);
            };
            visit(visit, treeRoot);
        }
        else if (traversalName == "POSTORDER")
        {
            auto visit = [&](auto&& self, int index) -> void
            {
                if (index == -1) return;
                self(self, treeNodes[index].left);
                self(self, treeNodes[index].right);
                order.push_back(index);
            };
            visit(visit, treeRoot);
        }
        else // LEVEL ORDER
        {
            if (treeRoot != -1)
            {
                std::queue<int> q;
                q.push(treeRoot);
                while (!q.empty())
                {
                    int index = q.front(); q.pop();
                    order.push_back(index);
                    if (treeNodes[index].left != -1) q.push(treeNodes[index].left);
                    if (treeNodes[index].right != -1) q.push(treeNodes[index].right);
                }
            }
        }

        if (order.empty())
        {
            algorithmStatus = traversalName + ": tree is empty.";
            return;
        }

        std::string sequence;
        for (int index : order)
        {
            if (!sequence.empty()) sequence += " , ";
            sequence += std::to_string(treeNodes[index].value);
        }
        algorithmStatus = traversalName + " TRAVERSAL: " + sequence;
        startTreeNodeAnimation(order);
    };

    auto calculateTreeHeightDiameter = [&]()
    {
        clearTreeAnimation();
        // Returns {height in edges, diameter in edges}.
        auto calculate = [&](auto&& self, int index) -> std::pair<int, int>
        {
            if (index == -1) return {-1, 0};
            auto left = self(self, treeNodes[index].left);
            auto right = self(self, treeNodes[index].right);
            int height = 1 + std::max(left.first, right.first);
            int throughNode = (left.first + 1) + (right.first + 1);
            int diameter = std::max({left.second, right.second, throughNode});
            return {height, diameter};
        };

        if (treeRoot == -1)
        {
            algorithmStatus = "TREE HEIGHT: 0 edges | DIAMETER: 0 edges (empty tree)";
            return;
        }

        auto result = calculate(calculate, treeRoot);
        algorithmStatus = "TREE HEIGHT: " + std::to_string(result.first) +
                          " edges | DIAMETER: " + std::to_string(result.second) + " edges";
    };

    // Input boxes
    sf::RectangleShape nodeInputBox({145.f, 40.f});
    nodeInputBox.setPosition({295.f, 250.f});
    nodeInputBox.setFillColor(Theme::Canvas);
    nodeInputBox.setOutlineThickness(2.f);
    nodeInputBox.setOutlineColor(Theme::Edge);

    sf::RectangleShape weightInputBox({145.f, 40.f});
    weightInputBox.setPosition({455.f, 250.f});
    weightInputBox.setFillColor(Theme::Canvas);
    weightInputBox.setOutlineThickness(2.f);
    weightInputBox.setOutlineColor(Theme::Edge);

    // Sidebar background behind the existing controls.
    sf::RectangleShape sidebarPanel({180.f, 700.f});
    sidebarPanel.setPosition({0.f, 0.f});
    sidebarPanel.setFillColor(sf::Color::Transparent);

    // Graph / BST canvas.
    sf::RectangleShape canvas({790.f, 560.f});
    canvas.setPosition({640.f, 90.f});
    canvas.setFillColor(sf::Color::Transparent);
    canvas.setOutlineThickness(0.f);
    canvas.setOutlineColor(Theme::Edge);

    // Result panel along the bottom of the canvas.
    sf::RectangleShape resultPanel({790.f, 90.f});
    resultPanel.setPosition({640.f, 670.f});
    resultPanel.setFillColor(sf::Color::Transparent);
    resultPanel.setOutlineThickness(0.f);
    resultPanel.setOutlineColor(Theme::Edge);

    sf::CircleShape nodeShape(25.f);
    nodeShape.setOrigin({25.f, 25.f});
    nodeShape.setFillColor(Theme::Canvas);
    nodeShape.setOutlineThickness(2.5f);
    nodeShape.setOutlineColor(Theme::Node);

    // Main loop
    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            // Close window
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            // Text input
            if (const auto* textEvent =
                    event->getIf<sf::Event::TextEntered>())
            {
                if (enteringNodeName)
                {
                    if (textEvent->unicode == 8)
                    {
                        if (!nodeNameInput.empty())
                            nodeNameInput.pop_back();
                    }
                    else if (textEvent->unicode >= 32 &&
                             textEvent->unicode < 127)
                    {
                        nodeNameInput +=
                            static_cast<char>(textEvent->unicode);
                    }
                }
                else if (enteringWeight)
                {
                    if (textEvent->unicode == 8)
                    {
                        if (!weightInput.empty())
                            weightInput.pop_back();
                    }
                    else if (textEvent->unicode >= '0' &&
                             textEvent->unicode <= '9')
                    {
                        weightInput +=
                            static_cast<char>(textEvent->unicode);
                    }
                }
                else if (treeMode && treeInputActive)
                {
                    if (textEvent->unicode == 8)
                    {
                        if (!treeInput.empty()) treeInput.pop_back();
                    }
                    else if ((textEvent->unicode >= '0' && textEvent->unicode <= '9') ||
                             (textEvent->unicode == '-' && treeInput.empty()))
                    {
                        treeInput += static_cast<char>(textEvent->unicode);
                    }
                }
            }

            // Keyboard commands
            if (const auto* key =
                    event->getIf<sf::Event::KeyPressed>())
            {
                // Finish node name
                if (key->code == sf::Keyboard::Key::Enter &&
                    enteringNodeName)
                {
                    if (!nodeNameInput.empty())
                    {
                        placingNode = true;
                        enteringNodeName = false;
                    }
                }

                // Finish weight input
                else if (key->code == sf::Keyboard::Key::Enter &&
                         enteringWeight)
                {
                    enteringWeight = false;
                }
                else if (key->code == sf::Keyboard::Key::Enter && treeInputActive)
                {
                    treeInputActive = false;
                }

                // Escape cancels current action
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    enteringNodeName = false;
                    placingNode = false;
                    selectingEdge = false;
                    enteringWeight = false;
                    deletingNode = false;
                    draggingNode = -1;
                    choosingDijkstraStart = false;
                    choosingDijkstraEnd = false;
                    dijkstraStart = -1;
                    treeInputActive = false;

                    firstSelectedNode = -1;
                    secondSelectedNode = -1;
                }
            }

            // Mouse movement: drag a node
            if (const auto* mouseMove =
                    event->getIf<sf::Event::MouseMoved>())
            {
                if (!treeMode && draggingNode != -1)
                {
                    sf::Vector2f mousePosition =
                        window.mapPixelToCoords(mouseMove->position);

                    sf::Vector2f newPosition{
                        mousePosition.x + dragOffset.x,
                        mousePosition.y + dragOffset.y
                    };

                    // Keep node inside the graph canvas
                    newPosition.x = std::clamp(
                        newPosition.x, 660.f, 1400.f);

                    newPosition.y = std::clamp(
                        newPosition.y, 110.f, 625.f);

                    nodes[draggingNode].position = newPosition;
                }
            }

            // Mouse release: stop dragging
            if (const auto* mouseRelease =
                    event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mouseRelease->button == sf::Mouse::Button::Left)
                {
                    draggingNode = -1;
                }
            }

            // Mouse click
            if (const auto* mouseClick =
                    event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouseClick->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f mousePosition =
                        window.mapPixelToCoords(mouseClick->position);

                    // Workspace navigation is handled by the left sidebar.
                    // The user can switch modes at any time without a separate Back button.
                    if (sf::FloatRect({25.f, 158.f}, {220.f, 48.f}).contains(mousePosition))
                    {
                        if (treeMode)
                        {
                            treeMode = false;
                            treeInputActive = false;
                            clearTreeAnimation();
                            draggingNode = -1;
                            enteringNodeName = false;
                            placingNode = false;
                            selectingEdge = false;
                            enteringWeight = false;
                            deletingNode = false;
                            algorithmStatus = "Graph Editor workspace selected.";
                        }
                    }
                    else if (sf::FloatRect({25.f, 214.f}, {220.f, 48.f}).contains(mousePosition))
                    {
                        if (!treeMode)
                        {
                            treeMode = true;
                            treeInputActive = false;
                            draggingNode = -1;
                            enteringNodeName = false;
                            placingNode = false;
                            selectingEdge = false;
                            enteringWeight = false;
                            deletingNode = false;
                            clearAlgorithmResult();
                            algorithmStatus = "BST mode: enter values and choose a tree operation.";
                        }
                    }
                    else if (treeMode)
                    {
                        if (treeInputBox.getGlobalBounds().contains(mousePosition))
                        {
                            treeInputActive = true;
                        }
                        else if (treeInsertButton.getGlobalBounds().contains(mousePosition))
                        {
                            treeInputActive = false;
                            if (!treeInput.empty())
                            {
                                try { insertTreeValue(std::stoi(treeInput)); treeInput.clear(); }
                                catch (...) { algorithmStatus = "Please enter a valid integer."; }
                            }
                            else algorithmStatus = "Type an integer in the input box first.";
                        }
                        else if (treeSearchButton.getGlobalBounds().contains(mousePosition))
                        {
                            treeInputActive = false;
                            if (!treeInput.empty())
                            {
                                try { searchTreeValue(std::stoi(treeInput)); treeInput.clear(); }
                                catch (...) { algorithmStatus = "Please enter a valid integer."; }
                            }
                            else algorithmStatus = "Type the value to search for first.";
                        }
                        else if (treeDeleteButton.getGlobalBounds().contains(mousePosition))
                        {
                            treeInputActive = false;
                            if (!treeInput.empty())
                            {
                                try { deleteTreeValue(std::stoi(treeInput)); treeInput.clear(); }
                                catch (...) { algorithmStatus = "Please enter a valid integer."; }
                            }
                            else algorithmStatus = "Type the value to delete first.";
                        }
                        else if (treeInorderButton.getGlobalBounds().contains(mousePosition))
                            runTreeTraversal("INORDER");
                        else if (treePreorderButton.getGlobalBounds().contains(mousePosition))
                            runTreeTraversal("PREORDER");
                        else if (treePostorderButton.getGlobalBounds().contains(mousePosition))
                            runTreeTraversal("POSTORDER");
                        else if (treeLevelButton.getGlobalBounds().contains(mousePosition))
                            runTreeTraversal("LEVEL ORDER");
                        else if (treeMetricsButton.getGlobalBounds().contains(mousePosition))
                            calculateTreeHeightDiameter();
                        else if (treeClearButton.getGlobalBounds().contains(mousePosition))
                        {
                            treeNodes.clear();
                            treeRoot = -1;
                            treeInput.clear();
                            clearTreeAnimation();
                            algorithmStatus = "BST cleared.";
                        }
                    }
                    else if (treeModeButton.getGlobalBounds().contains(mousePosition))
                    {
                        treeMode = true;
                        treeInputActive = false;
                        draggingNode = -1;
                        enteringNodeName = false;
                        placingNode = false;
                        selectingEdge = false;
                        enteringWeight = false;
                        deletingNode = false;
                        clearAlgorithmResult();
                        algorithmStatus = "BST mode: enter values and choose a tree operation.";
                    }
                    else
                    {
                    // Algorithm buttons
                    if (dijkstraButton.getGlobalBounds().contains(mousePosition))
                    {
                        clearAlgorithmResult();
                        choosingDijkstraStart = true;
                        algorithmStatus = "Dijkstra: click the start node.";
                    }
                    else if (primButton.getGlobalBounds().contains(mousePosition))
                    {
                        runPrim();
                    }
                    else if (kruskalButton.getGlobalBounds().contains(mousePosition))
                    {
                        runKruskal();
                    }

                    // Add Node button
                    else if (addNodeButton.getGlobalBounds()
                            .contains(mousePosition))
                    {
                        enteringNodeName = true;
                        placingNode = false;
                        selectingEdge = false;
                        enteringWeight = false;
                        deletingNode = false;
                        draggingNode = -1;

                        nodeNameInput.clear();
                        firstSelectedNode = -1;
                        secondSelectedNode = -1;
                    }

                    // Place Node button
                    else if (placeNodeButton.getGlobalBounds()
                                 .contains(mousePosition))
                    {
                        if (!nodeNameInput.empty())
                        {
                            placingNode = true;
                            enteringNodeName = false;
                        }
                    }

                    // Add Edge button
                    else if (addEdgeButton.getGlobalBounds()
                                 .contains(mousePosition))
                    {
                        selectingEdge = true;
                        placingNode = false;
                        enteringNodeName = false;
                        enteringWeight = false;
                        deletingNode = false;
                        draggingNode = -1;

                        firstSelectedNode = -1;
                        secondSelectedNode = -1;
                        weightInput.clear();
                        algorithmStatus = "LINK: click the first node, then the second; enter a weight and click CREATE EDGE (blank weight defaults to 1).";
                    }

                    // Create Edge button. If the user leaves weight blank, use 1
                    // as a safe default so the button still completes the link.
                    else if (createEdgeButton.getGlobalBounds()
                                 .contains(mousePosition))
                    {
                        if (firstSelectedNode == -1 || secondSelectedNode == -1)
                        {
                            algorithmStatus = "To create an edge: click LINK, select two different nodes, enter a weight, then click CREATE EDGE.";
                        }
                        else
                        {
                            int weight = 1;
                            bool validWeight = true;
                            if (!weightInput.empty())
                            {
                                try
                                {
                                    weight = std::stoi(weightInput);
                                    if (weight < 0) validWeight = false;
                                }
                                catch (...)
                                {
                                    validWeight = false;
                                }
                            }

                            if (!validWeight)
                            {
                                algorithmStatus = "Edge weight must be a non-negative integer.";
                            }
                            else
                            {
                                bool duplicate = false;
                                for (const Edge& edge : edges)
                                {
                                    if ((edge.from == firstSelectedNode && edge.to == secondSelectedNode) ||
                                        (edge.from == secondSelectedNode && edge.to == firstSelectedNode))
                                    {
                                        duplicate = true;
                                        break;
                                    }
                                }

                                if (duplicate)
                                {
                                    algorithmStatus = "These nodes are already connected.";
                                }
                                else
                                {
                                    edges.push_back({firstSelectedNode, secondSelectedNode, weight});
                                    clearAlgorithmResult();
                                    algorithmStatus = "Edge created with weight " + std::to_string(weight) + ". Run an algorithm again.";
                                }

                                firstSelectedNode = -1;
                                secondSelectedNode = -1;
                                selectingEdge = false;
                                enteringWeight = false;
                                weightInput.clear();
                            }
                        }
                    }

                    // Delete Node button
                    else if (deleteNodeButton.getGlobalBounds()
                                 .contains(mousePosition))
                    {
                        deletingNode = true;
                        placingNode = false;
                        selectingEdge = false;
                        enteringNodeName = false;
                        enteringWeight = false;
                        draggingNode = -1;

                        firstSelectedNode = -1;
                        secondSelectedNode = -1;
                    }

                    // Click graph canvas
                    else if (canvas.getGlobalBounds()
                                 .contains(mousePosition))
                    {
                        int clickedNode =
                            findNodeAtPosition(nodes, mousePosition);

                        // Dijkstra node picking: first start, then destination.
                        if (choosingDijkstraStart && clickedNode != -1)
                        {
                            dijkstraStart = clickedNode;
                            choosingDijkstraStart = false;
                            choosingDijkstraEnd = true;
                            algorithmStatus = "Dijkstra: now click destination node.";
                        }
                        else if (choosingDijkstraEnd && clickedNode != -1)
                        {
                            int destination = clickedNode;
                            choosingDijkstraEnd = false;
                            runDijkstra(dijkstraStart, destination);
                        }

                        // Place a newly named node
                        else if (placingNode)
                        {
                            mousePosition.y = std::clamp(mousePosition.y, 110.f, 625.f);
                            nodes.push_back({
                                nodeNameInput,
                                mousePosition
                            });

                            clearAlgorithmResult();
                            algorithmStatus = "Graph changed; run an algorithm again.";
                            nodeNameInput.clear();
                            placingNode = false;
                        }

                        // Delete clicked node and its connected edges
                        else if (deletingNode && clickedNode != -1)
                        {
                            // Remove edges connected to this node
                            edges.erase(
                                std::remove_if(
                                    edges.begin(),
                                    edges.end(),
                                    [clickedNode](const Edge& edge)
                                    {
                                        return edge.from == clickedNode ||
                                               edge.to == clickedNode;
                                    }
                                ),
                                edges.end()
                            );

                            // Fix indices of edges that refer to nodes
                            // after the deleted node
                            for (Edge& edge : edges)
                            {
                                if (edge.from > clickedNode)
                                    edge.from--;

                                if (edge.to > clickedNode)
                                    edge.to--;
                            }

                            // Remove the node itself
                            nodes.erase(nodes.begin() + clickedNode);

                            clearAlgorithmResult();
                            algorithmStatus = "Node deleted; run an algorithm again.";
                            deletingNode = false;
                        }

                        // Select two nodes to connect
                        else if (selectingEdge && clickedNode != -1)
                        {
                            if (firstSelectedNode == -1)
                            {
                                firstSelectedNode = clickedNode;
                            }
                            else if (secondSelectedNode == -1 &&
                                     clickedNode != firstSelectedNode)
                            {
                                secondSelectedNode = clickedNode;
                                enteringWeight = true;
                                weightInput.clear();
                            }
                        }

                        // Normal click on a node starts dragging
                        else if (clickedNode != -1)
                        {
                            draggingNode = clickedNode;

                            dragOffset = {
                                nodes[clickedNode].position.x -
                                    mousePosition.x,
                                nodes[clickedNode].position.y -
                                    mousePosition.y
                            };
                        }
                    }
                    } // end graph-mode click handling
                }
            }
        }

        // Animate BST search/traversal highlights one node every half-second.
        if (treeMode && treeAnimationRunning &&
            treeAnimationClock.getElapsedTime().asSeconds() >= 0.5f)
        {
            if (treeAnimationCursor < treeAnimationOrder.size())
            {
                int nextNode = treeAnimationOrder[treeAnimationCursor++];
                if (std::find(treeHighlightedNodes.begin(), treeHighlightedNodes.end(), nextNode) ==
                    treeHighlightedNodes.end())
                    treeHighlightedNodes.push_back(nextNode);
                treeAnimationClock.restart();
            }
            if (treeAnimationCursor >= treeAnimationOrder.size())
                treeAnimationRunning = false;
        }

        // Grow the green line from the already-green first endpoint.
        if (!treeMode && animationRunning && currentlyAnimatingEdge != -1)
        {
            currentEdgeProgress = std::min(
                animationClock.getElapsedTime().asSeconds() / 0.5f,
                1.f
            );

            if (currentEdgeProgress >= 1.f)
            {
                if (std::find(highlightedEdges.begin(),
                              highlightedEdges.end(),
                              currentlyAnimatingEdge) == highlightedEdges.end())
                {
                    highlightedEdges.push_back(currentlyAnimatingEdge);
                }

                animationRunning = false;
                nodeRevealPending = true;
                nodeRevealClock.restart();
            }
        }

        // After the line is completely green, wait 0.5 seconds, then turn
        // ONLY its second endpoint green. Then start the next edge.
        if (!treeMode && nodeRevealPending &&
            !animationEdgeOrder.empty() &&
            nodeRevealClock.getElapsedTime().asSeconds() >= 0.5f)
        {
            if (animatingToNode >= 0 &&
                std::find(highlightedNodes.begin(), highlightedNodes.end(),
                          animatingToNode) == highlightedNodes.end())
            {
                highlightedNodes.push_back(animatingToNode);
            }

            nodeRevealPending = false;
            animationEdgeCursor++;

            if (animationEdgeCursor < animationEdgeOrder.size())
            {
                prepareCurrentAnimationEdge();
            }
            else
            {
                currentlyAnimatingEdge = -1;
                animatingFromNode = -1;
                animatingToNode = -1;

                // Preserve any algorithm-only node (e.g. source==destination)
                // after the edge-by-edge animation has completed.
                for (int nodeIndex : animationNodeOrder)
                {
                    if (std::find(highlightedNodes.begin(),
                                  highlightedNodes.end(),
                                  nodeIndex) == highlightedNodes.end())
                    {
                        highlightedNodes.push_back(nodeIndex);
                    }
                }
            }
        }

        // If the result has no edges (e.g. Dijkstra source==destination),
        // reveal its relevant node(s) after a brief pause.
        if (!treeMode && nodeRevealPending && animationEdgeOrder.empty() &&
            nodeRevealClock.getElapsedTime().asSeconds() >= 0.5f)
        {
            highlightedNodes = animationNodeOrder;
            nodeRevealPending = false;
        }

        // Draw interface. Graph and BST modes share the canvas but keep data separate.
        window.clear(Theme::Background);
        // Three-column dashboard inspired by the supplied reference: navigation,
        // operation toolkit, and a large graph visualization workspace.
        drawRoundedPanel(window, sf::FloatRect({14.f, 14.f}, {242.f, 772.f}),
                         sf::Color(8, 10, 18), sf::Color(35, 43, 62), 16.f, 1.2f);
        drawRoundedPanel(window, sf::FloatRect({270.f, 90.f}, {350.f, 690.f}),
                         sf::Color(9, 11, 17), sf::Color(35, 43, 62), 14.f, 1.2f);
        drawRoundedPanel(window, sf::FloatRect({640.f, 90.f}, {790.f, 560.f}),
                         sf::Color(8, 7, 15), sf::Color(49, 43, 75), 14.f, 1.2f);
        sf::RectangleShape topBar({1190.f, 1.f});
        topBar.setPosition({260.f, 74.f});
        topBar.setFillColor(sf::Color(32, 38, 53));
        window.draw(topBar);
        sf::Text brand(font, "PROJECT", 25);
        brand.setFillColor(Theme::Text); brand.setStyle(sf::Text::Bold);
        brand.setPosition({38.f, 35.f}); window.draw(brand);
        sf::Text brandSub(font, "DSA VISUALIZER", 11);
        brandSub.setFillColor(Theme::Node); brandSub.setPosition({40.f, 68.f}); window.draw(brandSub);
        sf::Text topTitle(font, treeMode ? "Binary Search Tree" : "Graph Visualizer", 23);
        topTitle.setFillColor(Theme::Text); topTitle.setPosition({292.f, 24.f}); window.draw(topTitle);
        sf::Text topSub(font, treeMode ? "BST OPERATIONS" : "GRAPH EDITOR  /  ALGORITHM LAB", 11);
        topSub.setFillColor(sf::Color(135, 145, 166)); topSub.setPosition({294.f, 52.f}); window.draw(topSub);
        sf::Text navHead(font, "WORKSPACES", 11);
        navHead.setFillColor(sf::Color(111, 123, 146)); navHead.setPosition({40.f, 130.f}); window.draw(navHead);
        drawRoundedPanel(window, sf::FloatRect({25.f, 158.f}, {220.f, 48.f}),
                         treeMode ? sf::Color(8, 10, 18) : sf::Color(10, 43, 46),
                         treeMode ? sf::Color(35, 43, 62) : Theme::Node, 8.f, 1.f);
        sf::Text graphNav(font, "◇   GRAPH EDITOR", 14);
        graphNav.setFillColor(treeMode ? sf::Color(145, 154, 174) : Theme::Node);
        graphNav.setPosition({45.f, 173.f}); window.draw(graphNav);
        drawRoundedPanel(window, sf::FloatRect({25.f, 214.f}, {220.f, 48.f}),
                         treeMode ? sf::Color(36, 28, 59) : sf::Color(8, 10, 18),
                         treeMode ? Theme::Selected : sf::Color(35, 43, 62), 8.f, 1.f);
        sf::Text bstNav(font, "◎   BINARY SEARCH TREE", 13);
        bstNav.setFillColor(treeMode ? Theme::Selected : sf::Color(145, 154, 174));
        bstNav.setPosition({45.f, 229.f}); window.draw(bstNav);
        sf::Text navFooter(font, "●  SYSTEM READY", 11);
        navFooter.setFillColor(Theme::Green); navFooter.setPosition({40.f, 748.f}); window.draw(navFooter);
        drawRoundedPanel(window, sf::FloatRect({280.f, 98.f}, {330.f, 680.f}),
                         sf::Color(8, 10, 18), sf::Color(28, 39, 60), 12.f, 1.f);
        sf::Text toolkit(font, treeMode ? "BST OPERATION TOOLKIT" : "INTERACTION TOOLKIT", 12);
        toolkit.setFillColor(sf::Color(126, 137, 158)); toolkit.setPosition({295.f, 112.f}); window.draw(toolkit);

        // Subtle grid, drawn behind graph edges and nodes.
        for (float x = 640.f; x <= 1430.f; x += 30.f)
        {
            sf::RectangleShape gridLine({1.f, 560.f});
            gridLine.setPosition({x, 90.f});
            gridLine.setFillColor(sf::Color(Theme::Grid.r, Theme::Grid.g, Theme::Grid.b, 150));
            window.draw(gridLine);
        }
        for (float y = 90.f; y <= 650.f; y += 30.f)
        {
            sf::RectangleShape gridLine({790.f, 1.f});
            gridLine.setPosition({640.f, y});
            gridLine.setFillColor(sf::Color(Theme::Grid.r, Theme::Grid.g, Theme::Grid.b, 150));
            window.draw(gridLine);
        }

        if (!treeMode)
        {
            drawButton(addNodeButton, "ADD NODE");
            drawButton(placeNodeButton, "MOVE / PLACE");
            drawButton(addEdgeButton, "LINK");
            drawButton(createEdgeButton, "CREATE EDGE");
            drawButton(deleteNodeButton, "DELETE NODE");
            drawButton(dijkstraButton, "Dijkstra");
            drawButton(primButton, "Prim (MST)");
            drawButton(kruskalButton, "Kruskal (MST)");


            drawRoundedPanel(window, nodeInputBox.getGlobalBounds(), sf::Color(5, 7, 15),
                             enteringNodeName ? Theme::Node : sf::Color(53, 61, 83), 8.f, 1.5f);
            drawRoundedPanel(window, weightInputBox.getGlobalBounds(), sf::Color(5, 7, 15),
                             enteringWeight ? Theme::Node : sf::Color(53, 61, 83), 8.f, 1.5f);

            sf::Text nodeLabel(font, "NODE NAME", 12);
            nodeLabel.setFillColor(sf::Color(155, 170, 195));
            nodeLabel.setPosition({295.f, 224.f});
            window.draw(nodeLabel);

            sf::Text weightLabel(font, "EDGE WEIGHT", 12);
            weightLabel.setFillColor(sf::Color(155, 170, 195));
            weightLabel.setPosition({455.f, 224.f});
            window.draw(weightLabel);

            sf::Text nodeInputText(font, nodeNameInput, 18);
            nodeInputText.setFillColor(Theme::Text);
            nodeInputText.setPosition({302.f, 259.f});
            window.draw(nodeInputText);

            sf::Text weightInputText(font, weightInput, 18);
            weightInputText.setFillColor(Theme::Text);
            weightInputText.setPosition({462.f, 259.f});
            window.draw(weightInputText);

            sf::Text sectionAlgorithms(font, "ALGORITHM SELECTOR", 12);
            sectionAlgorithms.setFillColor(sf::Color(126, 137, 158));
            sectionAlgorithms.setPosition({295.f, 382.f});
            window.draw(sectionAlgorithms);

            sf::Text instructions(
                font,
                "Dijkstra: choose start, then target.\n"
                "Prim / Kruskal: build a minimum\nspanning tree.\n\n"
                "Cyan edges show the selected result.\n"
                "Esc cancels node selection.",
                11
            );
            instructions.setFillColor(sf::Color(220, 220, 220));
            instructions.setPosition({295.f, 552.f});
            window.draw(instructions);

            // Draw graph edges and their weights.
            for (int edgeIndex = 0; edgeIndex < static_cast<int>(edges.size()); ++edgeIndex)
            {
                const Edge& edge = edges[edgeIndex];
                sf::Vector2f from = nodes[edge.from].position;
                sf::Vector2f to = nodes[edge.to].position;

                bool isHighlighted =
                    std::find(highlightedEdges.begin(), highlightedEdges.end(), edgeIndex) !=
                    highlightedEdges.end();
                sf::Color edgeColor = isHighlighted ? Theme::ActiveEdge : Theme::Edge;

                // Muted blue-gray edges keep the highlighted algorithm path prominent.
                drawThickLine(window, from, to, edgeColor, 4.5f);

                if (animationRunning && edgeIndex == currentlyAnimatingEdge)
                {
                    sf::Vector2f animationStart = nodes[animatingFromNode].position;
                    sf::Vector2f animationFinish = nodes[animatingToNode].position;
                    sf::Vector2f movingEnd{
                        animationStart.x + (animationFinish.x - animationStart.x) * currentEdgeProgress,
                        animationStart.y + (animationFinish.y - animationStart.y) * currentEdgeProgress
                    };
                    // Match the animated segment to the thicker graph edges.
                    drawThickLine(window, animationStart, movingEnd, Theme::ActiveEdge, 4.5f);
                }

                sf::Vector2f middle((from.x + to.x) / 2.f, (from.y + to.y) / 2.f);
                float dx = to.x - from.x;
                float dy = to.y - from.y;
                float length = std::sqrt(dx * dx + dy * dy);
                if (length > 0.f)
                {
                    middle.x += (-dy / length) * 12.f;
                    middle.y += ( dx / length) * 12.f;
                }
                sf::Text weightText(font, std::to_string(edge.weight), 15);
                weightText.setFillColor(Theme::Text);
                centerText(weightText, middle);

                sf::FloatRect weightBounds = weightText.getGlobalBounds();
                sf::RectangleShape weightPill({
                    std::max(24.f, weightBounds.size.x + 12.f),
                    21.f
                });
                weightPill.setOrigin({
                    weightPill.getSize().x / 2.f,
                    weightPill.getSize().y / 2.f
                });
                weightPill.setPosition(middle);
                weightPill.setFillColor(sf::Color(8, 10, 20, 242));
                weightPill.setOutlineThickness(1.f);
                weightPill.setOutlineColor(isHighlighted ? Theme::Green : sf::Color(67, 67, 102));
                window.draw(weightPill);
                window.draw(weightText);
            }

            // Draw graph nodes.
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
            {
                nodeShape.setPosition(nodes[i].position);
                bool isAlgorithmNode =
                    std::find(highlightedNodes.begin(), highlightedNodes.end(), i) != highlightedNodes.end();
                bool isEmphasized = i == firstSelectedNode || i == secondSelectedNode ||
                    isAlgorithmNode || (choosingDijkstraEnd && i == dijkstraStart);

                // Soft neon halo behind the node, inspired by the reference UI.
                sf::CircleShape halo(29.f);
                halo.setOrigin({29.f, 29.f});
                halo.setPosition(nodes[i].position);
                halo.setFillColor(sf::Color::Transparent);
                halo.setOutlineThickness(7.f);
                halo.setOutlineColor(isEmphasized
                    ? sf::Color(167, 139, 250, 90)
                    : sf::Color(69, 200, 255, 55));
                window.draw(halo);

                nodeShape.setFillColor(Theme::Canvas);
                nodeShape.setOutlineColor(isAlgorithmNode ? Theme::Green :
                    ((i == firstSelectedNode || i == secondSelectedNode) ? Theme::Selected : Theme::Node));
                window.draw(nodeShape);

                sf::Text nameText(font, nodes[i].name, 18);
                nameText.setFillColor(Theme::Text);
                centerText(nameText, nodes[i].position);
                window.draw(nameText);
            }
        }
        else
        {
            // Draw BST edges and compute a readable in-order layout.
            std::vector<sf::Vector2f> treePositions(treeNodes.size(), {0.f, 0.f});
            int inorderPosition = 0;
            const float treeLeft = 675.f;
            const float treeRight = 1395.f;
            const float treeTop = 145.f;
            const float treeBottom = 610.f;
            const float horizontalStep = treeNodes.empty()
                ? 60.f
                : std::max(48.f, (treeRight - treeLeft) /
                    static_cast<float>(std::max<std::size_t>(1, treeNodes.size())));
            const float verticalStep = 82.f;

            auto layoutTree = [&](auto&& self, int index, int depth) -> void
            {
                if (index == -1 || index >= static_cast<int>(treeNodes.size()))
                    return;
                self(self, treeNodes[index].left, depth + 1);
                float x = std::min(treeRight, treeLeft + 28.f + inorderPosition * horizontalStep);
                float y = std::min(treeBottom, treeTop + depth * verticalStep);
                treePositions[index] = {x, y};
                ++inorderPosition;
                self(self, treeNodes[index].right, depth + 1);
            };
            layoutTree(layoutTree, treeRoot, 0);

            // Draw parent-child connections first so nodes appear on top.
            for (int i = 0; i < static_cast<int>(treeNodes.size()); ++i)
            {
                if (treePositions[i].x == 0.f && treePositions[i].y == 0.f)
                    continue; // unused slot after a BST deletion

                if (treeNodes[i].left != -1 &&
                    treePositions[treeNodes[i].left].x != 0.f)
                {
                    drawThickLine(window, treePositions[i],
                                  treePositions[treeNodes[i].left],
                                  sf::Color(83, 91, 120), 3.f);
                }
                if (treeNodes[i].right != -1 &&
                    treePositions[treeNodes[i].right].x != 0.f)
                {
                    drawThickLine(window, treePositions[i],
                                  treePositions[treeNodes[i].right],
                                  sf::Color(83, 91, 120), 3.f);
                }
            }

            // Draw BST nodes and traversal/search highlights.
            for (int i = 0; i < static_cast<int>(treeNodes.size()); ++i)
            {
                if (treePositions[i].x == 0.f && treePositions[i].y == 0.f)
                    continue;

                bool highlighted = std::find(treeHighlightedNodes.begin(),
                                             treeHighlightedNodes.end(), i) !=
                                   treeHighlightedNodes.end();
                sf::CircleShape halo(25.f);
                halo.setOrigin({25.f, 25.f});
                halo.setPosition(treePositions[i]);
                halo.setFillColor(sf::Color::Transparent);
                halo.setOutlineThickness(6.f);
                halo.setOutlineColor(highlighted
                    ? sf::Color(53, 242, 181, 105)
                    : sf::Color(167, 139, 250, 55));
                window.draw(halo);

                sf::CircleShape treeCircle(21.f);
                treeCircle.setOrigin({21.f, 21.f});
                treeCircle.setPosition(treePositions[i]);
                treeCircle.setFillColor(sf::Color(12, 13, 26));
                treeCircle.setOutlineThickness(2.5f);
                treeCircle.setOutlineColor(highlighted ? Theme::Green : Theme::Selected);
                window.draw(treeCircle);

                sf::Text valueText(font, std::to_string(treeNodes[i].value), 16);
                valueText.setFillColor(Theme::Text);
                centerText(valueText, treePositions[i]);
                window.draw(valueText);
            }

            // BST operation controls.
            drawButton(treeInsertButton, "INSERT");
            drawButton(treeSearchButton, "SEARCH");
            drawButton(treeInorderButton, "INORDER");
            drawButton(treePreorderButton, "PREORDER");
            drawButton(treePostorderButton, "POSTORDER");
            drawButton(treeLevelButton, "LEVEL ORDER");
            drawButton(treeMetricsButton, "HEIGHT / DIAMETER");
            drawButton(treeClearButton, "CLEAR TREE");
            drawButton(treeDeleteButton, "DELETE VALUE");

            drawRoundedPanel(window, treeInputBox.getGlobalBounds(),
                             sf::Color(5, 7, 15),
                             treeInputActive ? Theme::Selected : Theme::Edge,
                             8.f, 1.5f);
            sf::Text treeInputText(font, treeInput, 18);
            treeInputText.setFillColor(Theme::Text);
            treeInputText.setPosition({305.f, 181.f});
            window.draw(treeInputText);

            sf::Text treeHint(font,
                "Enter an integer, then choose an operation.\n"
                "Traversal visits are highlighted one at a time.", 11);
            treeHint.setFillColor(sf::Color(190, 195, 210));
            treeHint.setPosition({295.f, 500.f});
            window.draw(treeHint);
        }

        // Shared result/status panel.
        drawRoundedPanel(window, sf::FloatRect({640.f, 670.f}, {790.f, 90.f}),
                         sf::Color(8, 10, 18), sf::Color(49, 43, 75), 12.f, 1.2f);
        sf::Text statusHeading(font, "STATUS / RESULT", 11);
        statusHeading.setFillColor(Theme::Green);
        statusHeading.setPosition({660.f, 683.f});
        window.draw(statusHeading);

        sf::Text statusText(font, algorithmStatus, 14);
        statusText.setFillColor(Theme::Text);
        statusText.setPosition({660.f, 706.f});
        // Keep long algorithm messages within the result panel.
        while (statusText.getLocalBounds().size.x > 745.f &&
               statusText.getCharacterSize() > 10)
            statusText.setCharacterSize(statusText.getCharacterSize() - 1);
        window.draw(statusText);

        window.display();
    }

    return 0;
}
