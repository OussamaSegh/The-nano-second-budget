# Part II. Building Blocks {.part}

Every concurrent data structure is assembled from a small set of parts: a way to exclude other threads, a way to change shared state atomically, and a way to publish state to readers. This part builds each of them carefully, measures it, and shows where it breaks down.
