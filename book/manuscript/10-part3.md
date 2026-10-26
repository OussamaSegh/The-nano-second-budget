# Part III. Queues and Buffers {.part}

In a well-designed low-latency system, computation is cheap and communication is expensive. Most of the latency an event accumulates is spent moving between threads. This part is about the structures that move it: the single-producer single-consumer ring, multi-producer multi-consumer queues, the Disruptor's broadcast ring, and the memory management that keeps all of them off the allocator.
