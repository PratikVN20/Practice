#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>

// Class to update data
class DataUpdater {
public:
    DataUpdater(std::queue<int>& dataQueue, std::mutex& mtx, std::condition_variable& cv, std::queue<std::string>& eventQueue, std::mutex& eventMtx, bool& done)
        : dataQueue(dataQueue), mtx(mtx), cv(cv), eventQueue(eventQueue), eventMtx(eventMtx), done(done) {}

    void operator()() {
        for (int i = 1; i <= 100; ++i) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                dataQueue.push(i);
                eventMtx.lock();
                eventQueue.push("UpdateData: Updated to " + std::to_string(i));
                eventMtx.unlock();
            }
            cv.notify_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate some work
        }
        {
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
            cv.notify_all();
        }
    }

private:
    std::queue<int>& dataQueue;
    std::mutex& mtx;
    std::condition_variable& cv;
    std::queue<std::string>& eventQueue;
    std::mutex& eventMtx;
    bool& done;
};

// Class to print data
class DataPrinter {
public:
    DataPrinter(std::queue<int>& dataQueue, std::mutex& mtx, std::condition_variable& cv, std::queue<std::string>& eventQueue, std::mutex& eventMtx, bool& done)
        : dataQueue(dataQueue), mtx(mtx), cv(cv), eventQueue(eventQueue), eventMtx(eventMtx), done(done) {}

    void operator()() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return !dataQueue.empty() || done; });

            while (!dataQueue.empty()) {
                int data = dataQueue.front();
                dataQueue.pop();
                lock.unlock();
                std::cout << "PrintData: Data = " << data << std::endl;

                eventMtx.lock();
                eventQueue.push("PrintData: Printed " + std::to_string(data));
                eventMtx.unlock();

                lock.lock();
            }

            if (done && dataQueue.empty()) break;
        }
    }

private:
    std::queue<int>& dataQueue;
    std::mutex& mtx;
    std::condition_variable& cv;
    std::queue<std::string>& eventQueue;
    std::mutex& eventMtx;
    bool& done;
};

// Class to keep track of events
class EventKeeper {
public:
    EventKeeper(std::queue<std::string>& eventQueue, std::mutex& eventMtx, std::queue<int>& dataQueue, std::mutex& mtx, bool& done)
        : eventQueue(eventQueue), eventMtx(eventMtx), dataQueue(dataQueue), mtx(mtx), done(done) {}

    void operator()() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Check events periodically

            std::unique_lock<std::mutex> lock(eventMtx);
            while (!eventQueue.empty()) {
                std::string event = eventQueue.front();
                eventQueue.pop();
                std::cout << "EventKeeper: Event = " << event << std::endl;
            }
            lock.unlock();

            {
                std::lock_guard<std::mutex> dataLock(mtx);
                if (done && dataQueue.empty() && eventQueue.empty()) break;
            }
        }
    }

private:
    std::queue<std::string>& eventQueue;
    std::mutex& eventMtx;
    std::queue<int>& dataQueue;
    std::mutex& mtx;
    bool& done;
};

int main() {
    // Shared resources
    std::queue<int> dataQueue;
    std::queue<std::string> eventQueue;
    std::mutex mtx;
    std::mutex eventMtx;
    std::condition_variable cv;
    bool done = false;

    // Create objects for each class
    DataUpdater updater(dataQueue, mtx, cv, eventQueue, eventMtx, done);
    DataPrinter printer(dataQueue, mtx, cv, eventQueue, eventMtx, done);
    EventKeeper keeper(eventQueue, eventMtx, dataQueue, mtx, done);

    // Create threads
    std::thread updateThread(updater);
    std::thread printThread(printer);
    std::thread eventThread(keeper);

    // Wait for threads to finish
    updateThread.join();
    printThread.join();
    eventThread.join();

    return 0;
}
