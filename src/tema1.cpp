#include <iostream>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <atomic>

using namespace std;

struct ThreadArgs {
    pthread_barrier_t *barrier;
    vector<string> *files;
    int no_mappers;
    int no_reducers;
    int thread_id;
    vector<unordered_map<string, set<int>>> *partial_results;
    atomic<int> *current_file_index;
};

bool compare(const pair<string, vector<int>> &a, 
             const pair<string, vector<int>> &b) {
    //first compare by the number of file indexes
    //if they are equal, compare by the word
    if (a.second.size() != b.second.size()) {
        return a.second.size() > b.second.size();
    }
    return a.first < b.first;
}

// Extract words from a file
vector<string> extract_words(const string &filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        return {};
    }

    vector<string> words;
    string word;

    //go through each word in the file
    while (file >> word) {
        string normalized_word;
        for (char c : word) {

            //check if each character is a letter
            if (isalpha(c)) {

                //convert the letter to lowercase
                //so that it respects the format
                normalized_word += tolower(c);
            }
        }
        if (!normalized_word.empty()) {

            //add the new formatted word to the vector of strings
            words.push_back(normalized_word);
        }
    }

    return words;
}

void write_output(const vector<pair<string, vector<int>>> &sorted_results,
                  int reducer_id, ThreadArgs *args) {

    //go through each letter of the alphabet
    for (char letter = 'a'; letter <= 'z'; ++letter) {

        //if the letter is not assigned to the current reducer skip it
        if ((letter - 'a') % args->no_reducers != reducer_id)
            continue;

        //create the output file
        string filename(1, letter);
        filename += ".txt";

        ofstream out(filename);
        if (!out.is_open()) {
            cerr << "Error opening output file: " << filename << endl;
            continue;
        }

        //go through each sorted result and write it to the output file
        for (const auto &entry : sorted_results) {

            //check if the first letter of the word is
            //the same as the current letter
            if (entry.first[0] == letter) {
                out << entry.first << ":[";

                //go through the file indexes and write them to the output file
                for (int i = 0; i < entry.second.size(); ++i) {
                    out << entry.second[i];

                    //check if its not the last file index
                    //and add a space
                    if (i < entry.second.size() - 1) {
                        out << " ";
                    }
                }
                out << "]\n";
            }
        }
    }
}

void mapper(ThreadArgs *args) {
    //local result for each mapper
    //i used a set to avoid having duplicates
    unordered_map<string, set<int>> local_result;

    while (true) {
        //extract the current file index
        int file_index = (*args->current_file_index)++;
        if (file_index >= args->files->size())
            break;

        const string &filename = (*args->files)[file_index];

        //extract words in the required format
        vector<string> words = extract_words(filename);

        //ad each word with the corresponding file indexes
        //in the map, without worrying about duplicates
        for (const string &word : words) {
            local_result[word].insert(file_index + 1);
        }
    }

    //store the local result in the partial results from the struct
    (*args->partial_results)[args->thread_id] = local_result;
}

void reducer(ThreadArgs *args) {
    pthread_barrier_wait(args->barrier);

    //get the index of the reducer
    int reducer_id = args->thread_id - args->no_mappers;

    //used set so that there are no duplicates
    unordered_map<string, set<int>> merged_results;

    //go through each partial result from the mappers
    for (const auto &partial_result : *args->partial_results) {
        for (const auto &entry : partial_result) {
            char first_letter = entry.first[0];

            //assign a set of letters to each reducer
            if ((first_letter - 'a') % args->no_reducers == reducer_id) {

                //if the reducer is supposed to handle the letter
                //add all the unique file indexes to the set
                merged_results[entry.first].insert(entry.second.begin(), 
                                                   entry.second.end());
            }
        }
    }

    //i converted the merged results to a vector of pairs and also used
    //vector instead of set so that i can sort the results
    vector<pair<string, vector<int>>> sorted_results;
    for (const auto &entry : merged_results) {
        sorted_results.push_back({entry.first,
                                 vector<int>(entry.second.begin(), 
                                             entry.second.end())});
    }

    //sort the results based on the requirements
    sort(sorted_results.begin(), sorted_results.end(), compare);

    //write the output to the files
    write_output(sorted_results, reducer_id, args);
}

void *thread_func(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    //check whether the thread is a mapper or a reducer
    if (args->thread_id < args->no_mappers) {
        mapper(args);

        //wait for all mappers to finish
        pthread_barrier_wait(args->barrier);
    } else {
        reducer(args);
    }

    return nullptr;
}

int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(stderr, "Wrong number of arguments\n");
        return -1;
    }

    //read arguments from command line
    int no_mappers = atoi(argv[1]);
    int no_reducers = atoi(argv[2]);
    const char *input_file = argv[3];

    int no_files;
    vector<string> files;

    //read file names from input file
    ifstream in(input_file);
    if (!in.is_open()) {
        cerr << "Error opening input file" << endl;
        return -1;
    }

    in >> no_files;
    for (int i = 0; i < no_files; i++) {
        string filename;
        in >> filename;
        files.push_back(filename);
    }

    pthread_barrier_t barrier;

    //init barrier with M + R so that all
    //threads wait for all threads to finish
    pthread_barrier_init(&barrier, nullptr, no_mappers + no_reducers);

    vector<unordered_map<string, set<int>>> partial_results(no_mappers);
    atomic<int> current_file_index(0);

    vector<pthread_t> threads(no_mappers + no_reducers);
    vector<ThreadArgs> args(no_mappers + no_reducers);

    // create all the threads (M + R)
    for (int i = 0; i < no_mappers + no_reducers; ++i) {
        args[i] = {&barrier, &files, no_mappers, no_reducers,
                    i, &partial_results, &current_file_index};
        pthread_create(&threads[i], nullptr, thread_func, &args[i]);
    }

    //join all the threads
    for (int i = 0; i < no_mappers + no_reducers; ++i) {
        pthread_join(threads[i], nullptr);
    }

    pthread_barrier_destroy(&barrier);

    return 0;
}
