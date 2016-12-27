// USING ACQUIRE AND RELEASE SEMANTICS
#include<thread>
#include<atomic>
#include <cinttypes>
using namespace std;

#define RING_BUFFER_SIZE 1000
class lockless_ring_buffer_spsc
{
	public :
		
		lockless_ring_buffer_spsc()
		{
			write.store(0);
			read.store(0);
		}
		
		bool try_push(int64_t val)
		{
			const auto current_tail = write.load();
            const auto next_tail = increment(current_tail);
			if (current_tail - read.load(std::memory_order_acquire) <= size -1 ) // syncronises with read.store in pop thread
            {
                buffer[current_tail % size] = val;
                write.store(next_tail, std::memory_order_release);
                return true;
            }

            return false;  
		}
		
		void push(int64_t val)
		{
			while( ! try_push(val) );
		}
		
		bool try_pop(int64_t* pval)
		{
			const auto currentHead = read.load();
            
            if (currentHead == write.load(std::memory_order_acquire))
            {
                return false;
            }

            *pval = buffer[currentHead % size];
            read.store(increment(currentHead), std::memory_order_release); // syncronises with read.load in push thread

            return true;
		}
		
		int64_t pop()
		{
			int64_t ret;
			while( ! try_pop(&ret) );
			return ret;
		}
		
	private :
		std::atomic<int64_t> write ;
		std::atomic<int64_t> read;
		int64_t size = RING_BUFFER_SIZE;
		int64_t buffer[RING_BUFFER_SIZE];
		
		int64_t increment(int n)
        {
            return (n + 1);
        }
};

int main (int argc, char** argv)
{
	lockless_ring_buffer_spsc queue;
	
	std::thread write_thread( [&] () {
			 for(int i = 0; i<1000000; i++)
			 {
					queue.push(i);
			 }
		 }  // End of lambda expression
												);
	std::thread read_thread( [&] () {
			 for(int i = 0; i<1000000; i++)
			 {
					queue.pop();
			 }
		 }  // End of lambda expression
												);
	write_thread.join();
	read_thread.join();
	 
	 return 0;
}