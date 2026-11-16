// Self-test snippet: threads, shared state, thread identity, the machine's
// thread count, unordered reductions and heap order. Scanned under its own
// path (not library, not CLI), so every rule applies. Not compiled.
#include <thread>  // expect: thread
#include <future>  // expect: thread
#include <execution>  // expect: thread
#include <stop_token>  // expect: thread
#include <omp.h>  // expect: thread
#include <atomic>  // expect: shared-state
#include <mutex>  // expect: shared-state
#include <shared_mutex>  // expect: shared-state
#include <condition_variable>  // expect: shared-state
#include <semaphore>  // expect: shared-state
#include <latch>  // expect: shared-state
#include <barrier>  // expect: shared-state
#include <numeric>
#include <queue>

void threads(std::vector<int>& v) {
    std::thread t([] {});  // expect: thread
    std::jthread j([] {});  // expect: thread
    auto f = std::async(std::launch::async, [] { return 1; });  // expect: thread
    std::future<int> g;  // expect: thread
    std::promise<int> p;  // expect: thread
    std::packaged_task<int()> pt;  // expect: thread
    std::for_each(std::execution::par, v.begin(), v.end(), fn);  // expect: thread
    std::  // expect: thread
        thread split_over_lines;
#pragma omp parallel for  // expect: thread
    for (int i = 0; i < 4; ++i) {}
    pthread_create(&tid, nullptr, run, nullptr);  // expect: thread
    CreateThread(nullptr, 0, run, nullptr, 0, nullptr);  // expect: thread
}

void shared() {
    std::atomic<int> a{0};  // expect: shared-state
    std::atomic_ref<int> r(x);  // expect: shared-state
    std::atomic_flag fl;  // expect: shared-state
    a.load(std::memory_order_relaxed);  // expect: shared-state
    std::mutex m;  // expect: shared-state
    std::lock_guard<std::mutex> lk(m);  // expect: shared-state
    std::scoped_lock both(m, m2);  // expect: shared-state
    std::condition_variable cv;  // expect: shared-state
    std::counting_semaphore<4> sem(0);  // expect: shared-state
    std::call_once(flag, init);  // expect: shared-state
    _Atomic int c11 = 0;  // expect: shared-state
    __atomic_fetch_add(&x, 1, 0);  // expect: shared-state
    _InterlockedIncrement(&lx);  // expect: shared-state
}

void identity_and_count() {
    auto id = std::this_thread::get_id();  // expect: thread-identity
    std::this_thread::yield();  // expect: thread-identity
    std::thread::id tid;  // expect: thread-identity
    auto other = worker.get_id();  // expect: thread-identity
    thread_local int cache = 0;  // expect: thread-identity
    static __thread int gnu_tls;  // expect: thread-identity
    auto self = pthread_self();  // expect: thread-identity
    auto n = std::thread::hardware_concurrency();  // expect: hardware-concurrency
    auto m = sysconf(_SC_NPROCESSORS_ONLN);  // expect: hardware-concurrency
    GetSystemInfo(&info);  // expect: hardware-concurrency
}

std::int64_t reductions(std::vector<std::int64_t>& v, std::vector<std::int64_t>& out) {
    auto s = std::reduce(v.begin(), v.end());  // expect: unordered-reduction
    auto t = std::transform_reduce(v.begin(), v.end(), v.begin(), 0);  // expect: unordered-reduction
    std::inclusive_scan(v.begin(), v.end(), out.begin());  // expect: unordered-reduction
    std::exclusive_scan(v.begin(), v.end(), out.begin(), 0);  // expect: unordered-reduction
    return s + t;
}

void heaps(std::vector<Entry>& h) {
    std::priority_queue<Entry> q;  // expect: heap-order
    std::make_heap(h.begin(), h.end(), later);  // expect: heap-order
    std::push_heap(h.begin(), h.end(), later);  // expect: heap-order
    std::pop_heap(h.begin(), h.end(), later);  // expect: heap-order
    std::ranges::sort_heap(h, later);  // expect: heap-order
    // determinism-lint: allow(heap-order) -- comparator orders by (key, seq) with seq the unique push counter (P1-00 D6)
    std::priority_queue<Entry, std::vector<Entry>, ByKeyThenSeq> ok;
}
