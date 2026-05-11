````md
# 🌐 Multi-threaded Web Crawler Simulation (OS Project)

## 📘 Course Information
- **Course:** Operating Systems  
- **University:** FAST  
- **Instructor:** Mr. Ubaidullah  

## 👥 Team Members
- Hamza (24k-3065)  
- Abdul Rafay Ansari (24k-3095)  
- Muhammad Shahzain Khan (24k-3059)  

## 🚀 Project Overview
This project is a **Multi-threaded Web Crawler Simulation** developed in **C++** to demonstrate core Operating System concepts such as concurrency, synchronization, and resource management.

The system simulates how search engines crawl web pages by processing multiple URLs in parallel using threads, while ensuring safe access to shared resources.

## 🎯 Objectives
- Understand sequential vs parallel execution  
- Implement multi-threading using pthreads  
- Handle race conditions using mutex  
- Control execution using semaphores  
- Avoid duplicate processing using a visited set  
- Integrate real web fetching using libcurl  

## ⚙️ Technologies Used
- **Language:** C++  
- **OS:** Linux (Ubuntu)  

### Libraries:
- pthread → Thread management  
- semaphore.h → Concurrency control  
- libcurl → Web page fetching  
- STL (queue, set) → Data structures  

## 🏗️ Project Structure
web-crawler-os-project/
├── crawler.cpp            # Basic sequential crawler  
├── thread_test.cpp        # Thread learning and testing  
└── crawler_threads.cpp    # Final multi-threaded crawler system  

## 🔄 System Workflow
- Initialize shared URL queue with seed URLs  
- Create threads using pthreads  
- Each thread:
  - Locks shared data using mutex  
  - Fetches a URL from queue  
  - Checks and updates visited set  
  - Unlocks shared data  
  - Fetches web page using libcurl  
  - Adds new URLs (based on depth control)  
- Repeat until queue is empty  
- Display final statistics  

## 🧠 Key OS Concepts Implemented

### 🔒 Mutex (Mutual Exclusion)
Ensures only one thread accesses shared data at a time, preventing race conditions.

### 🚦 Semaphore
Limits the number of active threads to control resource usage.

### 🧵 Multithreading
Allows parallel execution of tasks for improved performance.

### 📦 Queue (FIFO)
Manages URL processing order.

### 🧠 Visited Set
Prevents duplicate URL processing and infinite loops.

## 📦 External Dependency

### libcurl
Used for performing real HTTP requests and downloading web page content.

#### Installation:
```bash
sudo apt install libcurl4-openssl-dev
````

## ▶️ How to Compile & Run

### Compile:

```bash
g++ crawler_threads.cpp -o crawler -lpthread -lcurl
```

### Run:

```bash
./crawler
```

## 🧪 Sample Output

```txt
[Thread 1] Fetching: https://example.com
[Thread 2] Fetching: https://example.org
Downloaded ...

[Thread 1] Fetching: https://example.com/page1
[Thread 2] Fetching: https://example.org/page2
Downloaded ...

===== STATS =====
Total processed: 4
```

## ⚖️ Sequential vs Multi-threaded

| Feature     | crawler.cpp | crawler_threads.cpp |
| ----------- | ----------- | ------------------- |
| Execution   | Sequential  | Parallel            |
| Speed       | Slower      | Faster              |
| Threads     | ❌ No        | ✅ Yes               |
| OS Concepts | ❌ No        | ✅ Yes               |

## 🔥 Key Improvements

* Introduced multi-threading for faster execution
* Resolved race conditions using mutex
* Controlled thread execution using semaphore
* Prevented duplicate processing using visited set
* Added depth control for structured crawling
* Integrated libcurl for real-world web data fetching

## 🎓 Conclusion

This project demonstrates how operating system concepts can be applied to build a safe, efficient, and scalable multi-threaded system. It highlights the importance of synchronization and controlled concurrency in real-world applications like web crawling.

## ⚠️ Academic Note

This project is developed strictly for educational purposes as part of the Operating Systems course.

```
```
