# Dos-Projecto: High-Frequency Matching Engine Simulator

**Team Members:** * Josh Hoeckendorf (GitHub: JoshHoeck)
* Josh King (GitHub: joshuaking18)

## Project Overview
This project simulates a high-frequency limit order book (LOB) matching engine. It processes Level 3 Market-By-Order tick data to compare the execution efficiency of a custom-built Priority Queue (Max/Min Heaps) against a custom-built Hash Map.

The goal of this project is to demonstrate the architectural trade-offs between the $O(\log n)$ insertion/range-matching of a Heap and the $O(1)$ exact-price matching of a Hash Table.

## Dataset
The engine uses tick-level data from the Polymarket exchange. To ensure the repository remains lightweight, a localized `sample_data.csv` (containing 100,000 sequential orders) is included in this repository. This was converted from a parquet to csv file via python, which derived from the dataset found at https://www.kaggle.com/datasets/marvingozo/polymarket-tick-level-orderbook-dataset/data using the file orderbook_2026-03-07.parquet.

## Prerequisites
To compile and run this engine, your system must have:
* A C++ compiler supporting **C++17** or higher.
* Standard libraries only.

## Build & Run Instructions

### Compiling via Command Line
1. Clone this repository.
2. Open a terminal and navigate to the project root directory.
3. Compile the code using the following command:
   ```bash
   g++ -std=c++17 main.cpp -o matching_engine