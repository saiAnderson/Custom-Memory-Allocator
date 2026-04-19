# Custom Memory Allocator (C)

## Overview
This project implements a dynamic memory allocator in C using an implicit free list and first-fit placement strategy.

The goal is to understand low-level memory management, block layout design, and allocation strategies.

---

## Features
- Implicit free list
- First-fit allocation
- Boundary-tag coalescing
- 16-byte alignment

## Block Layout

Each memory block contains:

- Header (size + allocation bit)
- Payload
- Footer (size + allocation bit)

[ Header | Payload | Footer ]

---

## Allocation Strategy

- Uses **first-fit** to find a free block
- Splits block if it is larger than required
- Maintains alignment using 16-byte boundary

---

## Free & Coalescing

- When a block is freed, adjacent blocks are checked
- If neighboring blocks are free, they are merged (coalescing)
- Uses boundary tags to efficiently locate neighboring blocks

## Testing

Includes a randomized stress test:
- 100k allocation/free operations
- Data pattern validation to detect memory corruption

---

## How to Build

```bash
make
```

## How to Run
./main