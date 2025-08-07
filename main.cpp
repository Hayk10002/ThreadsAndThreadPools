#include <iostream>
#include <random>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <format>
#include <span>
#include <mutex>
#include <numeric>
#include <queue>
#include <optional>
#include <condition_variable>
#include <future>
#include <memory>
#include <string_view>
#include <sstream>

int generateRandomNumber(int min = 1, int max = 100) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}


template<typename T>
class ConcurrentSummer
{
public:
    virtual void start(std::span<T> vec) = 0;
    virtual T get() = 0;
    virtual ~ConcurrentSummer() = default;

};

template<typename T>
class ThreadSummer : public ConcurrentSummer<T>
{
    T result{};
    std::optional<std::jthread> thread{};
public: 

    void start(std::span<T> vec) override
    {
        thread = std::jthread([this, vec]() {
            result = std::accumulate(vec.begin(), vec.end(), T{});
        });
    }


    T get() override
    {
        if (!thread || !thread->joinable()) throw std::runtime_error("Thread not running or already joined."); 
        thread->join();
        return result;
    }
};

template<typename T>
class ThreadPoolSummer : public ConcurrentSummer<T>
{
    struct Task
    {
        std::span<T> data;
        std::promise<T> result_promise;
    };

    struct Worker
    {
        std::jthread thread;
        std::queue<ThreadPoolSummer::Task> tasks{};
        std::condition_variable new_task_cv{};
        std::mutex mutex{};
        std::atomic_int cur_task_count{0};
        std::atomic_bool stop{false};

        Worker() : thread(&Worker::work, this) {}

        ~Worker()
        {
            stop.store(true, std::memory_order::relaxed);
            new_task_cv.notify_all();
            if (thread.joinable()) thread.join();
        }

        void work()
        {
            while (true)
            {
                std::unique_lock<std::mutex> lock(mutex);
                new_task_cv.wait(lock, [this] { return stop.load(std::memory_order::relaxed) || !tasks.empty(); });
                if (stop.load(std::memory_order::relaxed) && tasks.empty()) break;
                
                auto task = std::move(tasks.front());
                tasks.pop();
                
                lock.unlock();
                
                T sum = std::accumulate(task.data.begin(), task.data.end(), T{});

                task.result_promise.set_value(sum);
                cur_task_count.fetch_sub(1, std::memory_order::relaxed);
            }
        }
    };

    inline static std::vector<Worker> workers{(std::jthread::hardware_concurrency() > 0) ? std::jthread::hardware_concurrency() : 1};

    static void addTask(Task&& task)
    {
        int min_worker_index = 0;
        for (int i = 1; i < workers.size(); ++i) if (workers[i].cur_task_count.load(std::memory_order::relaxed) < workers[min_worker_index].cur_task_count.load(std::memory_order::relaxed))
            min_worker_index = i;

        {
            std::lock_guard<std::mutex> lock(workers[min_worker_index].mutex);
            workers[min_worker_index].tasks.push(std::move(task));
            workers[min_worker_index].cur_task_count.fetch_add(1, std::memory_order::relaxed);
        }

        workers[min_worker_index].new_task_cv.notify_one();
    }

    std::future<T> result_future;
public:
    void start(std::span<T> vec) override
    {
        std::promise<T> promise;
        result_future = promise.get_future();
        addTask(Task{vec, std::move(promise)});
    }

    T get() override
    {
        if (!result_future.valid()) 
            throw std::runtime_error("No result future available.");   
         
        return result_future.get();
    }
};

template<typename T>
class AsyncSummer : public ConcurrentSummer<T>
{
    std::future<T> result_future;
public:
    void start(std::span<T> vec) override
    {
        result_future = std::async([vec]() {
            return std::accumulate(vec.begin(), vec.end(), T{});
        });
    }

    T get() override
    {
        if (!result_future.valid()) 
            throw std::runtime_error("No result future available.");
        return result_future.get();
    }
};

template<typename T>
std::unique_ptr<ConcurrentSummer<T>> getConcurrentSummer(std::string_view type)
{
    if (type == "thread")       return std::make_unique<ThreadSummer<T>>();
    if (type == "threadpool")   return std::make_unique<ThreadPoolSummer<T>>();
    if (type == "async")        return std::make_unique<AsyncSummer<T>>();
    throw std::invalid_argument("Unknown concurrency type: " + std::string(type));
}

struct Value
{
    int min, max, step;

    Value(int min, int max, int step) : min(min), max(max), step(step) {}
    Value(int value) : min(value), max(value + 1), step(1) {}
};

Value parseValue(std::vector<std::string_view>& args)
{
    if (args.empty()) 
        throw std::invalid_argument("No value provided");

    std::string_view arg = args.front();
    args.erase(args.begin());

    if (arg.find(':') == std::string_view::npos) 
    {
        int value = std::stoi(std::string(arg));
        return Value(value);
    }

    size_t pos1 = arg.find(':');
    size_t pos2 = arg.find(':', pos1 + 1);

    if (pos2 == std::string_view::npos) 
        throw std::invalid_argument("Invalid value format, expected <value> or <min>:<max>:<step>");

    int min = std::stoi(std::string(arg.substr(0, pos1)));
    int max = std::stoi(std::string(arg.substr(pos1 + 1, pos2 - pos1 - 1)));
    int step = std::stoi(std::string(arg.substr(pos2 + 1)));

    if (min >= max) 
        throw std::invalid_argument("Invalid range: min must be less than max");

    if (step <= 0)
        throw std::invalid_argument("Invalid step: must be greater than 0");

    return Value(min, max, step);
}

struct Config
{
    bool log = false;
    Value vec_size = 1000000;
    Value batch_size = 1000;
    Value max_concurrency = std::thread::hardware_concurrency();
    std::string concurrency_type = "threadpool";

    void parseArgs(std::vector<std::string_view>& args)
    {
        while (!args.empty()) 
        {
            std::string_view arg = args.front();
            args.erase(args.begin());
            if (arg == "--help" || arg == "-h") 
            {
                std::cout << 
"Value = <number> or <min>:<max>:<step> (max is excluded)\n"
"Usage:\n"
"    main\n"
"        [-h | --help] Prints this message\n"
"        [-l | --log]  Prints more information (for debugging purposes)\n"
"        [-v | --vec-size <Value for vec size> (default is 1000000)]\n"
"        [-b | --batch-size <Value for batch size> (default is 1000)]\n"
"        [-m | --max-concurrency <Value for max concurrency> (default is std::thread::hardware_concurrency() and allowed maximum is --vec-size / --batch-size)]\n"
"        [-c | --concurrency-type thread|threadpool|async (default is threadpool)]\n";
            
                std::exit(0);
            }
            else if (arg == "--log" || arg == "-l") log = true;
            else if (arg == "--vec-size" || arg == "-v")
            {
                if (args.empty()) 
                    throw std::invalid_argument("No value provided for --vec-size");

                vec_size = parseValue(args);
            }
            else if (arg == "--batch-size" || arg == "-b") 
            {
                if (args.empty()) 
                    throw std::invalid_argument("No value provided for --batch-size");

                batch_size = parseValue(args);
            }
            else if (arg == "--max-concurrency" || arg == "-m")
            {
                if (args.empty()) 
                    throw std::invalid_argument("No value provided for --max-concurrency");

                max_concurrency = parseValue(args);

                if (max_concurrency.min <= 0)
                    throw std::invalid_argument("Max concurrency must always be greater than 0");
            }
            else if (arg == "--concurrency-type" || arg == "-c")
            {
                if (args.empty()) 
                    throw std::invalid_argument("No value provided for --concurrency-type");

                concurrency_type = std::string(args.front());

                if (concurrency_type != "thread" && concurrency_type != "threadpool" && concurrency_type != "async")
                    throw std::invalid_argument("Invalid concurrency type: " + concurrency_type);

                args.erase(args.begin());
            }
        }
    }
};

int num_len(int num)
{
    return std::floor(std::log10(num)) + 1;
}


volatile int sink = 0;
void benchmark(std::string_view concurrency_type, std::vector<int>& vec, int batch_size, int max_concurrency, bool log = false)
{
    int result = 0;
    std::vector<std::unique_ptr<ConcurrentSummer<int>>> summers;
    summers.reserve(max_concurrency);

    for (int i = 0; i < max_concurrency; ++i)
        summers.push_back(getConcurrentSummer<int>(concurrency_type));

    auto start = std::chrono::steady_clock::now();

    int i;
    for (i = 0;; i++)
    {
        int start = i * batch_size;
        if (start >= vec.size()) break; // No more batches

        int end = std::min(start + batch_size, static_cast<int>(vec.size()));
        std::span<int> batch(vec.data() + start, end - start);

        if (log) std::cout << "Summing elements from " << start << " to " << end - 1 << std::endl;

        if (i >= max_concurrency) result += summers[i % max_concurrency]->get(); // Wait for the previous batch to finish
        summers[i % max_concurrency]->start(batch);
    }
    
    for (int j = 0; j < max_concurrency; i++, j++)
        result += summers[i % max_concurrency]->get(); // Wait for remaining batches

    auto time = std::chrono::steady_clock::now() - start;
    std::cout << std::format("Time: {:>8}, Result: {:>{}}\n", 
                            std::chrono::duration_cast<std::chrono::microseconds>(time),
                            result, num_len(vec.size()) + 3);
}

// Value = <number> or <min>:<max>:<step>  
/* usage: 
    main 
        [-h | --help] // Prints this message
        [-l | --log]  // Prints more information (for debugging purposes)
        [-v | --vec-size <Value for vec size> (default is 1000000)] 
        [-b | --batch-size <Value for batch size> (default is 1000)]
        [-m | --max-concurrency <Value for max concurrency> (default is std::thread::hardware_concurrency() and allowed maximum is --vec-size / --batch-size)]
        [-c | --concurrency-type thread|threadpool|async (default is threadpool)]
*/
int main(int argc, char* argv[])
{
    std::vector<std::string_view> args(argv + 1, argv + argc);
    Config config;

    try
    {
        config.parseArgs(args);
    }
    catch (const std::invalid_argument& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::vector<int> vec(config.vec_size.min);
    std::generate(vec.begin(), vec.end(), []() { return generateRandomNumber(); });
    for (int vec_size = config.vec_size.min; vec_size < config.vec_size.max; vec_size += config.vec_size.step)
    {
        vec.resize(vec_size);
        std::generate(vec.begin() + vec_size - config.vec_size.step, vec.end(), []() { return generateRandomNumber(); });

        std::cout << "Starting benchmarks for vector size: " << vec_size << std::endl;

        for (int batch_size      = config.batch_size.min     ; batch_size      < config.batch_size.max     ; batch_size      += config.batch_size.step     )
        for (int max_concurrency = config.max_concurrency.min; max_concurrency < config.max_concurrency.max; max_concurrency += config.max_concurrency.step)
        {
            int real_max_concurrency = std::max(1, std::min(max_concurrency, (int)std::ceil((double)vec_size / batch_size)));
            std::cout << std::format("Batch size: {:>{}}, Max concurrency: {:>{}}, ", 
                            batch_size,           num_len(config.batch_size.max),
                            real_max_concurrency, num_len(config.max_concurrency.max));
            benchmark(config.concurrency_type, vec, batch_size, real_max_concurrency, config.log);

            // No need to go further, we can't have more concurrency than batches
            if (max_concurrency >= vec_size / batch_size) break;
        }

    }
    return 0;
}
