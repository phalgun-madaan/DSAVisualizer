# DSA Visualizer

A C++17 desktop application built with SFML 3 for interactively exploring graph algorithms and Binary Search Tree (BST) operations.

## Features

- Add, position, move, and delete named graph nodes
- Create weighted, undirected edges
- Dijkstra shortest-path visualization
- Prim and Kruskal minimum spanning tree (MST) visualizations
- Separate Binary Search Tree workspace
- BST insertion, search, deletion, and four traversal methods
- BST height and diameter calculations
- Animated highlights for algorithm results and tree traversals

## Requirements

- C++17-compatible compiler
- CMake 3.28 or newer
- SFML 3

## Build and run

### macOS (Homebrew)

1. Install Apple's command-line developer tools:

   ```bash
   xcode-select --install
   ```

2. Install Homebrew if it is not already installed: <https://brew.sh/>
3. Install CMake and SFML:

   ```bash
   brew install cmake sfml
   ```

4. Open Terminal in the repository folder (the folder containing `CMakeLists.txt`) and run:

   ```bash
   cmake -S . -B build
   cmake --build build
   ./build/dsa-visualizer
   ```

### Windows (Visual Studio 2022 + vcpkg)

1. Install Visual Studio 2022 and select the **Desktop development with C++** workload.
2. Install Git for Windows and CMake.
3. Install vcpkg and SFML 3. Open PowerShell and run:

   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg.exe install sfml:x64-windows
   ```

4. In PowerShell, change directory to the repository folder (the folder containing `CMakeLists.txt`), then configure and build:

   ```powershell
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

5. Run the application:

   ```powershell
   .\build\Release\dsa-visualizer.exe
   ```

## User guide: Graph Visualizer

When the application opens, the **Graph Editor** workspace is selected. The left side contains the controls; use the large grid/canvas on the right to place and work with nodes.

### 1. Add nodes

1. Click **ADD NODE**.
2. Type a node name using the keyboard. The text appears in the **NODE NAME** field.
3. Press **Enter**, or click **MOVE / PLACE**.
4. Move the pointer to the desired location on the graph canvas and click once to place the node.
5. Repeat these steps to add more nodes. Use a different name for each node.

### 2. Move a node

1. Make sure no add, link, or delete action is active. Press **Esc** to cancel an unfinished action if needed.
2. Click and hold a node, drag it to a new location, and release the mouse button.

### 3. Connect nodes with a weighted edge

1. Click **LINK**.
2. Click the first node on the canvas.
3. Click a different second node. The **EDGE WEIGHT** field becomes active.
4. Type a non-negative whole-number weight. Press **Enter** when finished.
5. Click **CREATE EDGE** to add the connection. If the weight field is left blank, the application uses a default weight of `1`.
6. Repeat to create more connections. The weight is displayed beside the edge.

The graph uses undirected connections. The same pair of nodes cannot be connected twice.

### 4. Delete a node

1. Click **DELETE NODE**.
2. Click the node you want to remove on the canvas.
3. The node and all edges connected to it are removed.

### 5. Find a shortest path with Dijkstra

1. Make sure the nodes you want to connect are joined by edges.
2. Click **Dijkstra**.
3. Click the desired **start** node when prompted.
4. Click the desired **destination** node when prompted.
5. The application calculates the shortest path and animates the selected route. If no route exists, the status message reports that no path was found.

### 6. Find a minimum spanning tree

1. Create a connected, weighted graph with at least one node.
2. Click **Prim (MST)** or **Kruskal (MST)**.
3. The selected minimum spanning tree is highlighted, and the result/status area reports the selected edges and total weight.

For a graph with multiple disconnected components, a spanning tree covering every node does not exist; connect the components before running either MST algorithm.

### 7. Cancel an unfinished action

Press **Esc** to cancel the current node-entry, placement, edge-selection, weight-entry, or deletion action.

## User guide: Binary Search Tree (BST)

1. Select **BINARY SEARCH TREE** from the workspace navigation on the left (or use the BST option in the Graph Editor).
2. Click the BST input field and type an integer. Negative integers are accepted.
3. Choose the operation you want:
   - **INSERT** — add the number to the BST.
   - **SEARCH** — find the number; the search path is highlighted.
   - **DELETE** — remove the number while maintaining BST ordering.
4. To run a traversal, click one of:
   - **INORDER** — left subtree, node, right subtree.
   - **PREORDER** — node, left subtree, right subtree.
   - **POSTORDER** — left subtree, right subtree, node.
   - **LEVEL ORDER** — visit nodes level by level.
5. Click **HEIGHT / DIAMETER** (the metrics control) to calculate and display the tree's height and diameter.
6. Click **CLEAR TREE** to remove all BST nodes and start again.

Duplicate values are ignored. Traversal and search highlights are animated in the tree view. Graph data and BST data are kept in separate workspaces.

## Understanding the project files

- `main.cpp` — application entry point, SFML window and interface, graph and BST data, mouse/keyboard interaction, algorithm implementations, and visual animations.
- `CMakeLists.txt` — CMake project configuration and SFML 3 linking.
- `README.md` — setup, build, and usage instructions.
- `.gitignore` — excludes generated build files and common local editor/system files from version control.

## Notes

- This project uses **SFML 3**. Do not substitute SFML 2; the APIs are not compatible.
- The source checks common Windows and macOS font locations when loading a font.
- The `build/` directory is generated locally by CMake. It does not need to be uploaded to GitHub.
- The source package is shared across platforms, but each user must build the executable on their own operating system with the appropriate compiler and SFML installation.
