# Persistent Deque

A persistent data structure keeps older versions of the data structures immutable.

This experimental repo contains an almost fully persistent deque implementation with a backbone loosely based on the std::deque implementation. 

What do I mean by almost fully persistent? Here we have 2 differences: 1) we only preserve versions that are in-scope (such as one thread holding a copy), and 2) the persistence check is a little unstable since it relies on checking if a shared pointer is shared by more than 1 user, as opposed doing something more stable such as using a lock or a CAS loop. Persistence is kept through copy-on-write, 

The implementation is inspired by the common std::deque implementation which uses a map of pointers to fixed size blocks. In our case we use a dynamic circular buffer as the map storing shared pointers to blocks, and a fixed circular buffer as the block. The std::deque implementation has a predetermined block size in bytes (i.e., 512 or 4096), but in our case we can alter the block size by setting a template parameter.

Performance characteristics:
- Copies: $\Theta(m)$, where $m$ is the number of blocks. We just copy the map of pointers.
- Push/Pop: $\Theta(1)$
- Value searches (assuming sorted deque): $\Theta(log\,n)$, where $n$ is the number of elements.

Single copy performance is still no where near the std::deque performance. I suspect it's largely due to using a circular buffer for the fixed size buffer, and not reusing freed buffers. I'm sure we can keep the map as a circular buffer with little downsides.
