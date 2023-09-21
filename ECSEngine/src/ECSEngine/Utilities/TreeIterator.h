#pragma once
#include "../Core.h"
#include "../Containers/Stacks.h"
#include "../Containers/Queues.h"

namespace ECSEngine {

	// Iterator Handler Interface - can copy paste
	// using StorageType = ;
	// 
	// using ReturnType = ;
	// 
	// Stream<StorageType> GetChildren(StorageType value, AllocatorPolymorphic allocator);
	// 
	// bool HasChildren(StorageType value) const;
	// 
	// Stream<StorageType> GetRoots(AllocatorPolymorphic allocator);
	// 
	// ReturnType GetReturnValue(StorageType value, AllocatorPolymorphic allocator);
	// 
	// void Free();

	struct IteratorPolymorphic {
		ECS_INLINE bool Valid() const {
			return valid(iterator);
		}

		ECS_INLINE void Next(void* return_type, unsigned int* level = nullptr) {
			next(iterator, return_type, level);
		}

		ECS_INLINE void Peek(void* return_type, unsigned int* level = nullptr, bool* has_children = nullptr) {
			peek(iterator, return_type, level, has_children);
		}

		ECS_INLINE void PeekStorage(void* storage_type) {
			peek_storage(iterator, storage_type);
		}

		ECS_INLINE void Has() {
			skip(iterator);
		}

		ECS_INLINE void Deallocate(const void* buffer) {
			deallocate(iterator, buffer);
		}

		void* iterator;
		bool (*valid)(const void* iterator);
		void (*next)(void* iterator, void* return_type, unsigned int* level);
		void (*peek)(const void* iterator, void* return_type, unsigned int* level, bool* has_children);
		void (*peek_storage)(const void* iterator, void* storage_type);
		void (*skip)(void* iterator);
		void (*deallocate)(void* iterator, const void* buffer);
		unsigned int storage_type_size;
		unsigned int return_type_size;
	};

	template<typename Type>
	IteratorPolymorphic IteratorToPolymorphic(Type* type) {
		IteratorPolymorphic iterator;

		iterator.iterator = type;

		auto valid = [](const void* iterator) {
			return ((Type*)iterator)->Valid();
		};

		auto next = [](void* iterator, void* return_type, unsigned int* level) {
			auto value = ((Type*)iterator)->Next(level);
			if (return_type != nullptr) {
				memcpy(return_type, &value, sizeof(value));
			}
		};

		auto peek = [](const void* iterator, void* return_type, unsigned int* level, bool* has_children) {
			auto value = ((Type*)iterator)->Peek(level, has_children);
			memcpy(return_type, &value, sizeof(value));
		};

		auto peek_storage = [](const void* iterator, void* storage_type) {
			auto storage = ((Type*)iterator)->PeekStorage();
			memcpy(storage_type, &storage, sizeof(storage));
		};

		auto skip = [](void* iterator) {
			((Type*)iterator)->Has();
		};

		auto deallocate = [](void* iterator, const void* buffer) {
			((Type*)iterator)->Deallocate(buffer);
		};

		iterator.deallocate = deallocate;
		iterator.next = next;
		iterator.peek = peek;
		iterator.peek_storage = peek_storage;
		iterator.skip = skip;
		iterator.valid = valid;
		iterator.storage_type_size = sizeof(typename Type::StorageType);
		iterator.return_type_size = sizeof(typename Type::ReturnType);

		return iterator;
	}

	// The handler needs to have as typedefs ReturnType and StorageType
	// As functions:	Stream<StorageType> GetChildren(StorageType, AllocatorPolymorphic allocator) - returns a stream of elements to be added from a certain node 
	// 					bool HasChildren(StorageType) const - returns true if the value has children, false otherwise
	//					(the stream returned needs to be stable, either from an internal allocator or from the one given)
	//					Stream<StorageType> GetRoots(AllocatorPolymorphic allocator) - returns the root nodes
	//					ReturnType GetReturnValue(StorageType) - returns a user-facing value from a node
	//					If bytes are allocated during GetReturnValue, its the caller's responsability to deallocate them or at the end of the iterator's usage
	//					to clear the allocator
	template<typename Handler>
	struct BFSIterator {
		using ReturnType = typename Handler::ReturnType;
		using StorageType = typename Handler::StorageType;

		struct Node {
			StorageType value;
			unsigned int level;
		};

		ECS_INLINE BFSIterator() {}

		ECS_INLINE BFSIterator(AllocatorPolymorphic allocator, Handler _handler, unsigned int default_capacity = 0) {
			Initialize(allocator, _handler, default_capacity);
		}

		ECS_INLINE bool Valid() const {
			return resizable_storage.GetSize() != 0;
		}

		// Pop the element from the queue and returns the value
		// Can optionally give a pointer to a uint to get its level in the hierarchy
		// Level 0 means it is a root
		ReturnType Next(unsigned int* level = nullptr) {
			Node current_value;
			resizable_storage.Pop(current_value);

			// Get the children of the current node
			Stream<StorageType> children = handler.GetChildren(current_value.value, resizable_storage.m_queue.allocator);
			for (size_t index = 0; index < children.size; index++) {
				resizable_storage.Push({ children[index], current_value.level + 1 });
			}

			// Return the user facing value
			if (level != nullptr) {
				*level = current_value.level;
			}
			return handler.GetReturnValue(current_value.value, resizable_storage.m_queue.allocator);
		}

		// Peek the next element without removing it
		// Can optionally give a pointer to a uint to get its level in the hierarchy
		// Level 0 means it is a root
		ReturnType Peek(unsigned int* level = nullptr, bool* has_children = nullptr) {
			Node current_value;
			resizable_storage.Peek(current_value);
			
			if (level != nullptr) {
				*level = current_value.level;
			}

			if (has_children != nullptr) {
				*has_children = handler.HasChildren(current_value.value);
			}

			return handler.GetReturnValue(current_value.value, resizable_storage.m_queue.allocator);
		}

		StorageType PeekStorage() const {
			Node node;
			resizable_storage.Peek(node);
			return node.value;
		}

		// Skips the next node without visiting its children
		ECS_INLINE void Has() {
			Node temp;
			resizable_storage.Pop(temp);
		}

		// Retrieves a new node from the handler
		void Roots() {
			Stream<StorageType> roots = handler.GetRoots(resizable_storage.m_queue.allocator);
			for (size_t index = 0; index < roots.size; index++) {
				resizable_storage.Push({ roots[index], 0 });
			}
			Deallocate(roots.buffer);
		}

		ECS_INLINE void Deallocate(const void* buffer) {
			ECSEngine::DeallocateIfBelongs(resizable_storage.m_queue.allocator, buffer);
		}

		// Frees the queue buffer and the handler
		void Free() {
			resizable_storage.m_queue.FreeBuffer();
			handler.Free();
		}

		void Initialize(AllocatorPolymorphic allocator, Handler _handler, unsigned int default_capacity = 0) {
			resizable_storage = ResizableQueue<Node>(allocator, default_capacity);
			handler = _handler;

			Roots();
		}

		ECS_INLINE IteratorPolymorphic AsPolymorphic() {
			return IteratorToPolymorphic(this);
		}

		ResizableQueue<Node> resizable_storage;
		Handler handler;
	};

	// The handler needs to have as typedefs ReturnType and StorageType
	// As functions:	Stream<StorageType> GetChildren(StorageType, AllocatorPolymorphic allocator) - returns a stream of elements to be added from a certain node 
	//					bool HasChildren(StorageType) const - returns true if the value has children, false otherwise
	//					(the stream returned needs to be stable, either from an internal allocator or from the one given)
	//					Stream<StorageType> GetRoots(AllocatorPolymorphic allocator) - returns the root nodes
	//					ReturnType GetReturnValue(StorageType, AllocatorPolymorphic allocator) - returns a user-facing value from a node
	//					If bytes are allocated during GetReturnValue, its the caller's responsability to deallocate them or at the end of the iterator's usage
	//					to clear the allocator
	template<typename Handler>
	struct DFSIterator {
		using ReturnType = typename Handler::ReturnType;
		using StorageType = typename Handler::StorageType;

		struct Node {
			StorageType value;
			unsigned int level;
		};

		ECS_INLINE DFSIterator() {}

		ECS_INLINE DFSIterator(AllocatorPolymorphic allocator, Handler _handler, unsigned int default_capacity = 0) {
			Initialize(allocator, _handler, default_capacity);
		}

		ECS_INLINE bool Valid() const {
			return stack_frames.GetElementCount() != 0;
		}

		ReturnType Next(unsigned int* level = nullptr) {
			Node current_node;
			stack_frames.Pop(current_node);

			// Get its children
			Stream<StorageType> children = handler.GetChildren(current_node.value, stack_frames.m_stack.allocator);
			PushChildren(children, current_node.level);

			if (level != nullptr) {
				*level = current_node.level;
			}
			return handler.GetReturnValue(current_node.value, stack_frames.m_stack.allocator);
		}

		// Returns the next node without popping it from the stack
		ReturnType Peek(unsigned int* level = nullptr, bool* has_children = nullptr) {
			Node current_node;
			stack_frames.Peek(current_node);

			if (level != nullptr) {
				*level = current_node.level;
			}

			if (has_children != nullptr) {
				*has_children = handler.HasChildren(current_node.value);
			}

			return handler.GetReturnValue(current_node.value, stack_frames.m_stack.allocator);
		}

		StorageType PeekStorage() const {
			Node node;
			stack_frames.Peek(node);
			return node.value;
		}

		// Skips the next node, without visiting its children
		ECS_INLINE void Has() {
			Node temp;
			stack_frames.Pop(temp);
		}

		// Retrieves the root nodes
		void Roots() {
			Stream<StorageType> roots = handler.GetRoots(stack_frames.m_stack.allocator);
			// After the increment the roots will have the level 0
			PushChildren(roots, -1);

			Deallocate(roots.buffer);
		}

		void PushChildren(Stream<StorageType> children, unsigned int parent_level) {
			// Push them in reverse order such that they appear in the normal order
			for (size_t index = 0; index < children.size; index++) {
				stack_frames.Push({ children[children.size - 1 - index], parent_level + 1 });
			}
		}

		ECS_INLINE void Deallocate(const void* buffer) {
			ECSEngine::DeallocateIfBelongs(stack_frames.m_stack.allocator, buffer);
		}

		// Deallocates the stack buffer and the handler
		void Free() {
			stack_frames.m_stack.FreeBuffer();
			handler.Free();
		}

		void Initialize(AllocatorPolymorphic allocator, Handler _handler, unsigned int default_capacity = 0) {
			stack_frames = ResizableStack<Node>(allocator, default_capacity);
			handler = _handler;

			Roots();
		}

		ECS_INLINE IteratorPolymorphic AsPolymorphic() {
			return IteratorToPolymorphic(this);
		}

		ResizableStack<Node> stack_frames;
		Handler handler;
	};

}