#pragma once
#include "../Core.h"
#include "../Containers/Stacks.h"
#include "../Containers/Queues.h"

namespace ECSEngine {

	// Iterator Handler Interface - can copy paste
	// using storage_type = ;
	// 
	// using return_type = ;
	// 
	// Stream<storage_type> GetChildren(storage_type value, AllocatorPolymorphic allocator);
	// 
	// bool HasChildren(storage_type value) const;
	// 
	// Stream<storage_type> GetRoots(AllocatorPolymorphic allocator);
	// 
	// return_type GetReturnValue(storage_type value, AllocatorPolymorphic allocator);
	// 
	// void Free();

	struct IteratorPolymorphic {
		inline bool Valid() const {
			return valid(iterator);
		}

		inline void Next(void* return_type, unsigned int* level = nullptr) {
			next(iterator, return_type, level);
		}

		inline void Peek(void* return_type, unsigned int* level = nullptr, bool* has_children = nullptr) {
			peek(iterator, return_type, level, has_children);
		}

		inline void PeekStorage(void* storage_type) {
			peek_storage(iterator, storage_type);
		}

		inline void Skip() {
			skip(iterator);
		}

		inline void Deallocate(const void* buffer) {
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
			((Type*)iterator)->Skip();
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
		iterator.storage_type_size = sizeof(typename Type::storage_type);
		iterator.return_type_size = sizeof(typename Type::return_type);

		return iterator;
	}

	// The handler needs to have as typedefs return_type and storage_type
	// As functions:	Stream<storage_type> GetChildren(storage_type, AllocatorPolymorphic allocator) - returns a stream of elements to be added from a certain node 
	// 					bool HasChildren(storage_type) const - returns true if the value has children, false otherwise
	//					(the stream returned needs to be stable, either from an internal allocator or from the one given)
	//					Stream<storage_type> GetRoots(AllocatorPolymorphic allocator) - returns the root nodes
	//					return_type GetReturnValue(storage_type) - returns a user-facing value from a node
	//					If bytes are allocated during GetReturnValue, its the caller's responsability to deallocate them or at the end of the iterator's usage
	//					to clear the allocator
	template<typename Handler>
	struct BFSIterator {
		using return_type = typename Handler::return_type;
		using storage_type = typename Handler::storage_type;

		struct Node {
			storage_type value;
			unsigned int level;
		};

		BFSIterator() {}

		BFSIterator(AllocatorPolymorphic allocator, Handler _handler, unsigned int default_capacity = 0) {
			Initialize(allocator, _handler, default_capacity);
		}

		bool Valid() const {
			return resizable_storage.GetSize() != 0;
		}

		// Pop the element from the queue and returns the value
		// Can optionally give a pointer to a uint to get its level in the hierarchy
		// Level 0 means it is a root
		return_type Next(unsigned int* level = nullptr) {
			Node current_value;
			resizable_storage.Pop(current_value);

			// Get the children of the current node
			Stream<storage_type> children = handler.GetChildren(current_value.value, resizable_storage.m_queue.allocator);
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
		return_type Peek(unsigned int* level = nullptr, bool* has_children = nullptr) {
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

		storage_type PeekStorage() const {
			Node node;
			resizable_storage.Peek(node);
			return node.value;
		}

		// Skips the next node without visiting its children
		void Skip() {
			Node temp;
			resizable_storage.Pop(temp);
		}

		// Retrieves a new node from the handler
		void Roots() {
			Stream<storage_type> roots = handler.GetRoots(resizable_storage.m_queue.allocator);
			for (size_t index = 0; index < roots.size; index++) {
				resizable_storage.Push({ roots[index], 0 });
			}
			Deallocate(roots.buffer);
		}

		void Deallocate(const void* buffer) {
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

		IteratorPolymorphic AsPolymorphic() {
			return IteratorToPolymorphic(this);
		}

		ResizableQueue<Node> resizable_storage;
		Handler handler;
	};

	// The handler needs to have as typedefs return_type and storage_type
	// As functions:	Stream<storage_type> GetChildren(storage_type, AllocatorPolymorphic allocator) - returns a stream of elements to be added from a certain node 
	//					bool HasChildren(storage_type) const - returns true if the value has children, false otherwise
	//					(the stream returned needs to be stable, either from an internal allocator or from the one given)
	//					Stream<storage_type> GetRoots(AllocatorPolymorphic allocator) - returns the root nodes
	//					return_type GetReturnValue(storage_type, AllocatorPolymorphic allocator) - returns a user-facing value from a node
	//					If bytes are allocated during GetReturnValue, its the caller's responsability to deallocate them or at the end of the iterator's usage
	//					to clear the allocator
	template<typename Handler>
	struct DFSIterator {
		using return_type = typename Handler::return_type;
		using storage_type = typename Handler::storage_type;

		struct Node {
			storage_type value;
			unsigned int level;
		};

		DFSIterator() {}

		DFSIterator(AllocatorPolymorphic allocator, Handler _handler, unsigned int default_capacity = 0) {
			Initialize(allocator, _handler, default_capacity);
		}

		bool Valid() const {
			return stack_frames.GetElementCount() != 0;
		}

		return_type Next(unsigned int* level = nullptr) {
			Node current_node;
			stack_frames.Pop(current_node);

			// Get its children
			Stream<storage_type> children = handler.GetChildren(current_node.value, stack_frames.m_stack.allocator);
			PushChildren(children, current_node.level);

			if (level != nullptr) {
				*level = current_node.level;
			}
			return handler.GetReturnValue(current_node.value, stack_frames.m_stack.allocator);
		}

		// Returns the next node without popping it from the stack
		return_type Peek(unsigned int* level = nullptr, bool* has_children = nullptr) {
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

		storage_type PeekStorage() const {
			Node node;
			stack_frames.Peek(node);
			return node.value;
		}

		// Skips the next node, without visiting its children
		void Skip() {
			Node temp;
			stack_frames.Pop(temp);
		}

		// Retrieves the root nodes
		void Roots() {
			Stream<storage_type> roots = handler.GetRoots(stack_frames.m_stack.allocator);
			// After the increment the roots will have the level 0
			PushChildren(roots, -1);

			Deallocate(roots.buffer);
		}

		void PushChildren(Stream<storage_type> children, unsigned int parent_level) {
			// Push them in reverse order such that they appear in the normal order
			for (size_t index = 0; index < children.size; index++) {
				stack_frames.Push({ children[children.size - 1 - index], parent_level + 1 });
			}
		}

		void Deallocate(const void* buffer) {
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

		IteratorPolymorphic AsPolymorphic() {
			return IteratorToPolymorphic(this);
		}

		ResizableStack<Node> stack_frames;
		Handler handler;
	};

}