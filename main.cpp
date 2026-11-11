#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cstdint>
#include <algorithm>
#include <string>

using namespace std;


// Method 1: i -> t -> j
void multiply_itj(
    const vector<int>& A,
    const vector<int>& B,
    vector<int64_t>& C,
    int n, int m, int k,
    int start_row, int end_row,
    atomic<int>& completed_rows
) {
    for (int i = start_row; i < end_row; i++) {

        for (int t = 0; t < m; t++) {

            int a = A[i * m + t];

            for (int j = 0; j < k; j++) {
                C[i * k + j] +=
                    static_cast<int64_t>(a) * B[t * k + j];
            }
        }

        completed_rows.fetch_add(1, memory_order_relaxed);
    }
}


// Method 2: i -> j -> t
void multiply_ijt(
    const vector<int>& A,
    const vector<int>& B,
    vector<int64_t>& C,
    int n, int m, int k,
    int start_row, int end_row,
    atomic<int>& completed_rows
) {
    for (int i = start_row; i < end_row; i++) {

        for (int j = 0; j < k; j++) {

            int64_t sum = 0;

            for (int t = 0; t < m; t++) {
                sum += static_cast<int64_t>(A[i * m + t])
                     * B[t * k + j];
            }

            C[i * k + j] = sum;
        }

        completed_rows.fetch_add(1, memory_order_relaxed);
    }
}


int main(int argc, char* argv[]) {

    int n = 0;
    int m = 0;
    int k = 0;
    int T = 0;
    int method = 0;


    // Read parameters
    try {
        if (argc == 6) {
            n = stoi(argv[1]);
            m = stoi(argv[2]);
            k = stoi(argv[3]);
            T = stoi(argv[4]);
            method = stoi(argv[5]);
        }
        else if (argc == 1) {

            cout << "Enter n, m, k, number of threads and method(1:i -> t -> j; 2:i -> j -> t): ";

            if (!(cin >> n >> m >> k >> T >> method)) {
                cout << "Invalid input.\n";
                return 1;
            }
        }
        else {
            cout << "Usage: matmul n m k T method\n";
            return 1;
        }
    }
    catch (...) {
        cout << "Invalid input.\n";
        return 1;
    }


    // Validate parameters
    if (n <= 0 || m <= 0 || k <= 0 || T <= 0) {
        cout << "All parameters must be positive.\n";
        return 1;
    }

    if (method != 1 && method != 2) {
        cout << "Method must be 1 or 2.\n";
        return 1;
    }


    // Select multiplication function
    using MultiplyFunction = void (*)(
        const vector<int>&,
        const vector<int>&,
        vector<int64_t>&,
        int, int, int,
        int, int,
        atomic<int>&
    );

    MultiplyFunction selected_multiply;

    if (method == 1) {
        selected_multiply = multiply_itj;
        cout << "Selected method: i -> t -> j\n";
    }
    else {
        selected_multiply = multiply_ijt;
        cout << "Selected method: i -> j -> t\n";
    }


    // Create matrices
    vector<int> A;
    vector<int> B;
    vector<int64_t> C;

    try {
        A.resize(static_cast<size_t>(n) * m);
        B.resize(static_cast<size_t>(m) * k);
        C.resize(static_cast<size_t>(n) * k, 0);
    }
    catch (...) {
        cout << "Not enough memory to create matrices.\n";
        return 1;
    }


    // Generate random numbers
    mt19937 gen(42);
    uniform_int_distribution<int> dist(0, 9);

    for (int& x : A) {
        x = dist(gen);
    }

    for (int& x : B) {
        x = dist(gen);
    }


    cout << "\nMatrices generated successfully.\n";
    cout << "Matrix A: " << n << " x " << m << '\n';
    cout << "Matrix B: " << m << " x " << k << '\n';


    // Limit the number of threads to the number of rows
    int actual_threads = min(T, n);

    cout << "Threads: " << actual_threads << '\n';


    // Shared progress counter
    atomic<int> completed_rows(0);

    vector<thread> workers;


    // Start measuring time
    auto start_time = chrono::steady_clock::now();


    // Distribute rows between threads
    int base_rows = n / actual_threads;
    int extra_rows = n % actual_threads;

    int current_row = 0;


    for (int i = 0; i < actual_threads; i++) {

        int rows = base_rows;

        if (i < extra_rows) {
            rows++;
        }

        int start_row = current_row;
        int end_row = start_row + rows;

        current_row = end_row;


        workers.emplace_back(
            [&, start_row, end_row]() {
                selected_multiply(
                    A, B, C,
                    n, m, k,
                    start_row, end_row,
                    completed_rows
                );
            }
        );
    }


    // Display calculation progress
    cout << "\nProgress: 0%\n";

    int last_percent = 0;

    while (completed_rows.load(memory_order_relaxed) < n) {

        int current_percent =
            completed_rows.load(memory_order_relaxed) * 100 / n;

        if (current_percent != last_percent) {
            cout << "Progress: " << current_percent << "%\n";
            last_percent = current_percent;
        }

        this_thread::sleep_for(chrono::milliseconds(100));
    }


    // Wait for all threads
    for (thread& worker : workers) {
        worker.join();
    }


    // Stop measuring time
    auto end_time = chrono::steady_clock::now();

    double elapsed_time =
        chrono::duration<double>(end_time - start_time).count();


    cout << "Progress: 100%\n";


    // Calculate checksum
    int64_t checksum = 0;

    for (int64_t value : C) {
        checksum += value;
    }


    // Print results
    cout << "\nCalculation completed.\n";
    cout << "Execution time: " << elapsed_time << " seconds\n";
    cout << "Checksum: " << checksum << '\n';


    return 0;
}