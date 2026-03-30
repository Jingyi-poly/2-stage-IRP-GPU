"""
Data loading module for Prize-Collecting TSP instances.

This module provides the PCTSPData class for reading and parsing PCTSP
instance files from the data folder.

Data File Format:
- Line 1: Number of nodes (N)
- Lines 2 to N+1: Node info as "x,y prize" (coordinates and prize value)
  - Node 0 is the depot with prize = 0
- Lines N+2 to 2N+1: N×N distance matrix (space-separated floats)
"""

from typing import List, Tuple
import os


class PCTSPData:
    """
    A class to load and store Prize-Collecting TSP instance data.

    Attributes:
        filename (str): Name of the loaded instance file
        n_nodes (int): Number of nodes in the instance
        coordinates (List[Tuple[int, int]]): List of (x, y) coordinates for each node
        prizes (List[int]): Prize/reward value for each node (depot has prize 0)
        distance_matrix (List[List[float]]): N×N matrix of distances between nodes
        depot (int): Index of the depot node (always 0)
    """

    def __init__(self, filepath: str):
        """
        Initialize PCTSPData by loading an instance file.

        Args:
            filepath: Path to the .dat instance file

        Raises:
            FileNotFoundError: If the file does not exist
            ValueError: If the file format is invalid
        """
        self.filepath = filepath
        self.filename = os.path.basename(filepath)
        self.depot = 0  # Node 0 is always the depot

        self._load_instance(filepath)

    def _load_instance(self, filepath: str) -> None:
        """
        Parse the instance file and populate data attributes.

        Args:
            filepath: Path to the .dat instance file
        """
        with open(filepath, 'r') as f:
            lines = f.readlines()

        # Parse number of nodes (line 1)
        self.n_nodes = int(lines[0].strip())

        # Parse node coordinates and prizes (lines 2 to N+1)
        self.coordinates: List[Tuple[int, int]] = []
        self.prizes: List[int] = []

        for i in range(1, self.n_nodes + 1):
            line = lines[i].strip()
            # Format: "x,y prize"
            parts = line.split()
            coord_part = parts[0]  # "x,y"
            prize = int(parts[1])  # prize value

            # Parse coordinates
            x, y = coord_part.split(',')
            self.coordinates.append((int(x), int(y)))
            self.prizes.append(prize)

        # Parse distance matrix (lines N+2 to 2N+1)
        self.distance_matrix: List[List[float]] = []

        for i in range(self.n_nodes + 1, 2 * self.n_nodes + 1):
            line = lines[i].strip()
            distances = [float(d) for d in line.split()]
            self.distance_matrix.append(distances)

    def get_distance(self, i: int, j: int) -> float:
        """
        Get the distance between nodes i and j.

        Args:
            i: Source node index
            j: Destination node index

        Returns:
            Distance from node i to node j
        """
        return self.distance_matrix[i][j]

    def get_prize(self, node: int) -> int:
        """
        Get the prize value for a node.

        Args:
            node: Node index

        Returns:
            Prize value for the node
        """
        return self.prizes[node]

    def get_total_prize(self) -> int:
        """
        Get the total prize available (sum of all node prizes).

        Returns:
            Sum of all prizes
        """
        return sum(self.prizes)

    def get_nodes(self) -> List[int]:
        """
        Get list of all node indices.

        Returns:
            List of node indices [0, 1, ..., n_nodes-1]
        """
        return list(range(self.n_nodes))

    def get_customers(self) -> List[int]:
        """
        Get list of customer node indices (excluding depot).

        Returns:
            List of customer indices [1, 2, ..., n_nodes-1]
        """
        return list(range(1, self.n_nodes))

    def __repr__(self) -> str:
        """String representation of the instance."""
        return (f"PCTSPData(file='{self.filename}', "
                f"n_nodes={self.n_nodes}, "
                f"total_prize={self.get_total_prize()})")

    def summary(self) -> str:
        """
        Get a detailed summary of the instance.

        Returns:
            Multi-line string with instance details
        """
        lines = [
            f"Instance: {self.filename}",
            f"Number of nodes: {self.n_nodes}",
            f"Depot: Node {self.depot}",
            f"Total available prize: {self.get_total_prize()}",
            f"Prize range: {min(self.prizes[1:])} - {max(self.prizes[1:])}",
        ]
        return "\n".join(lines)
