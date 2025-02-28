# Inverted Index with Map-Reduce 

## Mara Fichios 331CA

### Overview
In this project I implemented a **parallel inverted index** using the
**Map-Reduce paradigm** in C++ with Pthreads. The inverted index maps unique
words from a set of input files to the files in which they appear. Results
are written to output files grouped by the first letter of each word. The
program divides the workload into **mapper threads**, which process input
files to extract words, and **reducer threads**, which aggregate results
and produce sorted outputs.

### Structure of the project
1. Parsing the input and the initialization:
- I first parse the input arguments, initialize the barrier and create the
threads.
    
2. Mapping Stage:
- I format the words extracted from the files and associate to each unique word
its IDs. Each local result is stored in an unordered_map with the word as key
and a set of IDs as value to avoid having duplicates. The local result is then
passed to the partial_results from the ThreadArgs struct so that the reducers
can process it later, after the barrier is reached. The result would be
something like {word, {ID1, ID2, ...}}.

3. Reducing Stage:
- By checking each partial result, each reducer gets to work with a set of
letters depending on the number of reducers and on the initial letter of the
word in each partial result. This way, the workload is evenly distributed among
the reducers so that later on each reducer inserts the words and IDs in the
merged results unordered map. The result would be something like
{word, {ID1, ID2, ...}}, but now all the IDs are gathered in one set. In order
to be able to sort the results, I converted the map and the set to vectors, so
that I can sort them based on the number of IDs and then lexicographically.

4. Writing the output and finalizing:
- Results are written to separate files (a.txt, b.txt, etc.), with each file
corresponding to a letter of the alphabet. After all reducers finish their job,
the threads are joined and the barrier is destroyed.

### Structures and functions
ThreadArgs struct:
- pthread_barrier_t *barrier (synchronization barrier that I used to ensure
    that all mappers finish their job before the reducers start)
- vector<string> (list of input files)
- int no_mappers (number of mapper threads)
- int no_reducers (number of reducer threads)
- int thread_id (id of the current thread that helps to determine if it is a
    mapper or a reducer)
- vector<unordered_map<string, set<int>>> *partial_results (stores the partial
    results of the mappers without duplicates)
- atomic<int> *current_file_index (index of the file that the current mapper
    should process; the atomic type is an efficient way to access a shared 
    variable without needing to use locks)

Functions:
- main()
    * Reads input files and initializes threads.
    * Waits for all threads to finish.
    * Destroys the barrier
- thread_func()
    * Determines if the current thread is a mapper or a reducer.
    * Executes the corresponding stage based on the thread_id.
- mapper()
    * Gets the files to process from the ThreadArgs struct.
    * Extracts and formats the words with extract_words().
    * Stores the result in an unordered_map.
    * Once a mapper finishes, it waits for the others to finish.
- reducer()
    * Waits for all mappers to finish.
    * Merges the partial results from the mappers grouped by the first letter
        of the word.
    * Sorts the words with the help of compare().
- write_output()
    * Writes the results to the output files corresponding to the first letter
        of the word.
- extract_words()
    * Extracts words by formatting them to lowercase and removing punctuation.
- compare()
    * Compares two pairs of strings based on the number of unique IDs and then
        lexicographically.