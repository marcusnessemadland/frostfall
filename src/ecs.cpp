/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/ecs.h>
#include <frost/bufferstorage.h>
#include <frost/math.h>
#include <frost/string.h>

namespace frost
{
	// Counted storage adapter. DynamicStorageT owns allocation and relocates live elements.
	template<typename Ty>
	struct EcsArrayT
	{
		FROST_CLASS(EcsArrayT
			, NO_COPY
			);

		explicit EcsArrayT(AllocatorI* _allocator)
			: m_count(0)
		{
			m_storage.setAllocator(_allocator);
		}

		~EcsArrayT()
		{
			clear();
		}

		Ty& operator[](uint32_t _idx)
		{
			return *m_storage.getPtr(_idx);
		}

		const Ty& operator[](uint32_t _idx) const
		{
			return *m_storage.getPtr(_idx);
		}

		void reserve(uint32_t _count)
		{
			if (_count > m_storage.getCapacity() )
			{
				m_storage.setCapacity(max(_count, max(16u, m_storage.getCapacity()*2) ), m_count);
			}
		}

		void push(Ty _value)
		{
			reserve(m_count + 1);
			FROST_PLACEMENT_NEW(m_storage.getPtr(m_count++), Ty)(frost::move(_value) );
		}

		void pop()
		{
			m_storage.getPtr(--m_count)->~Ty();
		}

		void clear()
		{
			while (0 != m_count)
			{
				pop();
			}
		}

		DynamicStorageT<Ty> m_storage;
		uint32_t            m_count;
	};

	struct EcsValue
	{
		const EcsType* m_type;
		void*          m_data;
		uint64_t       m_added;
		uint64_t       m_changed;
	};

	static void* ecsMoveValue(
		  AllocatorI* _allocator
		, const EcsType* _type
		, void* _value
		)
	{
		void* ptr = frost::alloc(_allocator, _type->size, _type->align);
		FROST_ASSERT(NULL != ptr, "Failed to allocate ECS value.");
		_type->move(ptr, _value);
		return ptr;
	}

	static void ecsDropValue(
		  AllocatorI* _allocator
		, const EcsType* _type
		, void* _ptr
		)
	{
		_type->destroy(_ptr);
		frost::free(_allocator, _ptr, _type->align);
	}

	struct EcsLocal
	{
		EcsLocal*      m_next;
		const EcsType* m_type;
		void*          m_data;
	};

	static void ecsDropLocals(AllocatorI* _allocator, void* _locals)
	{
		EcsLocal* ptr = (EcsLocal*)_locals;
		while (NULL != ptr)
		{
			EcsLocal* next = ptr->m_next;
			ecsDropValue(_allocator, ptr->m_type, ptr->m_data);
			frost::deleteObject(_allocator, ptr);
			ptr = next;
		}
	}

	struct EcsSlot
	{
		struct Status
		{
			///
			enum Enum
			{
				Free,
				Reserved,
				Alive,

				Count
			};
		};
		EcsSlot()
			: m_generation(1)
			, m_next(UINT32_MAX)
			, m_status(Status::Free)
		{
		}

		uint32_t     m_generation;
		uint32_t     m_next;
		Status::Enum m_status;
		Entity       m_parent;
		Entity       m_child;
		Entity       m_sibling;
	};

	struct EcsRow
	{
		Entity   m_entity;
		uint64_t m_added;
		uint64_t m_changed;
	};

	struct EcsRequired
	{
		uint32_t      m_pool;
		EcsRequiredFn m_insert;
	};

	struct EcsPool
	{
		EcsPool(AllocatorI* _allocator, const EcsType* _type)
			: m_allocator(_allocator)
			, m_type(_type)
			, m_sparse(_allocator)
			, m_rows(_allocator)
			, m_required(_allocator)
			, m_data(NULL)
			, m_capacity(0)
			, m_hook(NULL)
			, m_userData(NULL)
			, m_removed(NULL)
		{
		}

		~EcsPool()
		{
			for (uint32_t ii = 0; ii < m_rows.m_count; ++ii)
			{
				m_type->destroy(at(ii) );
			}

			frost::free(m_allocator, m_data, m_type->align);
		}

		void* at(uint32_t _idx)
		{
			return m_data + size_t(_idx)*m_type->size;
		}

		uint32_t find(Entity _entity) const
		{
			if (_entity.idx < m_sparse.m_count)
			{
				const uint32_t idx = m_sparse[_entity.idx];
				if (idx < m_rows.m_count && m_rows[idx].m_entity == _entity)
				{
					return idx;
				}
			}
			return UINT32_MAX;
		}

		void grow(uint32_t _entity)
		{
			while (m_sparse.m_count <= _entity)
			{
				m_sparse.push(UINT32_MAX);
			}

			if (m_rows.m_count == m_capacity)
			{
				const uint32_t next = max(16u, m_capacity*2);
				uint8_t* ptr = (uint8_t*)frost::alloc(m_allocator, size_t(next)*m_type->size, m_type->align);
				FROST_ASSERT(NULL != ptr, "Failed to grow ECS component storage.");
				for (uint32_t ii = 0; ii < m_rows.m_count; ++ii)
				{
					m_type->move(ptr + size_t(ii)*m_type->size, at(ii) );
					m_type->destroy(at(ii) );
				}

				frost::free(m_allocator, m_data, m_type->align);
				m_data = ptr;
				m_capacity = next;
			}
		}

		void erase(uint32_t _idx)
		{
			m_sparse[m_rows[_idx].m_entity.idx] = UINT32_MAX;
			m_type->destroy(at(_idx) );
			const uint32_t last = m_rows.m_count - 1;
			if (last != _idx)
			{
				m_type->move(at(_idx), at(last) );
				m_type->destroy(at(last) );
				m_rows[_idx] = m_rows[last];
				m_sparse[m_rows[_idx].m_entity.idx] = _idx;
			}

			m_rows.pop();
		}

		AllocatorI*            m_allocator;
		const EcsType*         m_type;
		EcsArrayT<uint32_t>    m_sparse;
		EcsArrayT<EcsRow>      m_rows;
		EcsArrayT<EcsRequired> m_required;
		uint8_t*               m_data;
		uint32_t               m_capacity;
		EcsComponentHookFn     m_hook;
		void*                  m_userData;
		EcsRequiredFn          m_removed;
	};

	struct EcsCommand
	{
		EcsCommand*     m_next;
		const EcsType*  m_type;
		void*           m_data;
		EcsCommandRunFn m_run;
	};

	struct EcsMessage
	{
		const EcsType* m_type;
		void*          m_data;
		uint64_t       m_sequence;
		uint64_t       m_frame;
	};

	struct EcsObserver
	{
		const EcsType* m_event;
		const EcsType* m_type;
		void*          m_data;
		EcsObserverFn  m_run;
		Entity         m_target;
		bool           m_active;
	};

	struct EcsSystem
	{
		EcsSystem()
			: m_type(NULL)
			, m_data(NULL)
			, m_run(NULL)
			, m_ready(NULL)
			, m_locals(NULL)
			, m_lastRun(0)
			, m_set(UINT32_MAX)
			, m_condition(NULL)
			, m_userData(NULL)
			, m_exclusive(false)
		{
		}

		const EcsType*    m_type;
		void*             m_data;
		EcsSystemFn       m_run;
		EcsSystemReadyFn  m_ready;
		void*             m_locals;
		uint64_t          m_lastRun;
		uint32_t          m_set;
		EcsRunConditionFn m_condition;
		void*             m_userData;
		bool              m_exclusive;
	};

	struct EcsEdge
	{
		uint32_t m_from;
		uint32_t m_to;
		bool     m_fromSet;
		bool     m_toSet;
	};

	struct EcsSet
	{
		explicit EcsSet(uint32_t _id)
			: m_id(_id)
			, m_condition(NULL)
			, m_userData(NULL)
		{
		}

		uint32_t          m_id;
		EcsRunConditionFn m_condition;
		void*             m_userData;
	};

	struct EcsPlugin
	{
		const EcsType* m_type;
		PluginI*       m_plugin;
		void*          m_allocation;
	};

	struct EcsAppSchedule
	{
		EcsAppSchedule(
			  const EcsType* _label
			, Schedule* _schedule
			, void* _state = NULL
			, bool _enter = false
			, EcsStateEqualFn _equal = NULL
			)
			: m_label(_label)
			, m_schedule(_schedule)
			, m_state(_state)
			, m_enter(_enter)
			, m_equal(_equal)
		{
		}

		const EcsType*  m_label;
		Schedule*       m_schedule;
		void*           m_state;
		bool            m_enter;
		EcsStateEqualFn m_equal;
	};

	struct EcsStateDriver
	{
		const EcsType* m_type;
		void*          m_data;
		EcsStateFn     m_run;
	};

	struct World::Impl
	{
		explicit Impl(AllocatorI* _allocator)
			: m_allocator(_allocator)
			, m_slots(_allocator)
			, m_pools(_allocator)
			, m_resources(_allocator)
			, m_messages(_allocator)
			, m_observers(_allocator)
			, m_head(NULL)
			, m_tail(NULL)
			, m_free(UINT32_MAX)
			, m_count(0)
			, m_iterations(0)
			, m_dispatch(0)
			, m_tick(1)
			, m_lastClear(0)
			, m_lastRun(0)
			, m_frame(0)
			, m_sequence(0)
			, m_systemAllocator(NULL)
			, m_flushing(false)
			, m_inSystem(false)
		{
		}

		~Impl()
		{
			while (NULL != m_head)
			{
				EcsCommand* next = m_head->m_next;
				ecsDropValue(m_allocator, m_head->m_type, m_head->m_data);
				frost::deleteObject(m_allocator, m_head);
				m_head = next;
			}

			for (uint32_t ii = 0; ii < m_observers.m_count; ++ii)
			{
				if (NULL != m_observers[ii].m_data)
				{
					ecsDropValue(m_allocator, m_observers[ii].m_type, m_observers[ii].m_data);
				}
			}

			for (uint32_t ii = 0; ii < m_messages.m_count; ++ii)
			{
				ecsDropValue(m_allocator, m_messages[ii].m_type, m_messages[ii].m_data);
			}

			for (uint32_t ii = 0; ii < m_pools.m_count; ++ii)
			{
				frost::deleteObject(m_allocator, m_pools[ii]);
			}

			for (uint32_t ii = 0; ii < m_resources.m_count; ++ii)
			{
				ecsDropValue(m_allocator, m_resources[ii].m_type, m_resources[ii].m_data);
			}
		}

		bool writable() const
		{
			return 0 == m_iterations && 0 == m_dispatch;
		}

		void collectObservers()
		{
			if (0 != m_dispatch)
			{
				return;
			}

			for (uint32_t ii = 0; ii < m_observers.m_count; ++ii)
			{
				EcsObserver& observer = m_observers[ii];
				if (!observer.m_active && NULL != observer.m_data)
				{
					ecsDropValue(m_allocator, observer.m_type, observer.m_data);
					observer.m_data = NULL;
				}
			}
		}

		bool valid(Entity _entity, EcsSlot::Status::Enum _status) const
		{
			return _entity.idx < m_slots.m_count
				&& m_slots[_entity.idx].m_generation == _entity.generation
				&& m_slots[_entity.idx].m_status == _status
				;
		}

		void callHook(
			  World& _world
			, EcsPool& _pool
			, Entity _entity
			, EcsLifecycle::Enum _event
			)
		{
			if (NULL != _pool.m_hook)
			{
				++m_dispatch;
				_pool.m_hook(_world, _entity, _event, _pool.m_userData);
				--m_dispatch;
			}
		}

		bool reaches(uint32_t _from, uint32_t _target) const
		{
			if (_from == _target)
			{
				return true;
			}
			const EcsArrayT<EcsRequired>& required = m_pools[_from]->m_required;
			for (uint32_t ii = 0; ii < required.m_count; ++ii)
			{
				if (reaches(required[ii].m_pool, _target) )
				{
					return true;
				}
			}
			return false;
		}

		AllocatorI*            m_allocator;
		EcsArrayT<EcsSlot>     m_slots;
		EcsArrayT<EcsPool*>    m_pools;
		EcsArrayT<EcsValue>    m_resources;
		EcsArrayT<EcsMessage>  m_messages;
		EcsArrayT<EcsObserver> m_observers;
		EcsCommand*            m_head;
		EcsCommand*            m_tail;
		uint32_t               m_free;
		uint32_t               m_count;
		uint32_t               m_iterations;
		uint32_t               m_dispatch;
		uint64_t               m_tick;
		uint64_t               m_lastClear;
		uint64_t               m_lastRun;
		uint64_t               m_frame;
		uint64_t               m_sequence;
		AllocatorI*            m_systemAllocator;
		bool                   m_flushing;
		bool                   m_inSystem;
	};

	World::World(AllocatorI* _allocator)
	{
		FROST_ASSERT(NULL != _allocator, "ECS requires an allocator.");
		m_impl = FROST_NEW(_allocator, Impl)(_allocator);
	}

	World::~World()
	{
		frost::deleteObject(m_impl->m_allocator, m_impl);
	}

	AllocatorI* World::getAllocator() const
	{
		return m_impl->m_allocator;
	}

	uint32_t World::getEntityCount() const
	{
		return m_impl->m_count;
	}

	bool World::contains(Entity _entity) const
	{
		return m_impl->valid(_entity, EcsSlot::Status::Alive);
	}

	Entity EcsAccess::reserve(World& _world)
	{
		World::Impl* self = _world.m_impl;
		uint32_t idx = self->m_free;
		if (UINT32_MAX == idx)
		{
			idx = self->m_slots.m_count;
			self->m_slots.push(EcsSlot());
		}
		else
		{
			self->m_free = self->m_slots[idx].m_next;
		}
		EcsSlot& slot = self->m_slots[idx];
		slot.m_status = EcsSlot::Status::Reserved;
		slot.m_next = UINT32_MAX;
		return Entity(idx, slot.m_generation);
	}

	Entity EcsAccess::create(World& _world)
	{
		if (!_world.m_impl->writable() )
		{
			return Entity();
		}
		Entity id = reserve(_world);
		activate(_world, id);
		return id;
	}

	bool EcsAccess::activate(World& _world, Entity _entity)
	{
		World::Impl* self = _world.m_impl;
		if (!self->writable() || !self->valid(_entity, EcsSlot::Status::Reserved) )
		{
			return false;
		}
		self->m_slots[_entity.idx].m_status = EcsSlot::Status::Alive;
		++self->m_count;
		return true;
	}

	uint32_t EcsAccess::pool(
		  World& _world
		, const EcsType* _type
		, bool _create
		)
	{
		World::Impl* self = _world.m_impl;
		for (uint32_t ii = 0; ii < self->m_pools.m_count; ++ii)
		{
			if (self->m_pools[ii]->m_type == _type)
			{
				return ii;
			}
		}

		if (!_create)
		{
			return UINT32_MAX;
		}
		const uint32_t idx = self->m_pools.m_count;
		self->m_pools.push(FROST_NEW(self->m_allocator, EcsPool)(self->m_allocator, _type) );
		return idx;
	}

	void* EcsAccess::component(
		  World& _world
		, uint32_t _pool
		, Entity _entity
		, bool _mark
		)
	{
		if (!_world.contains(_entity) || UINT32_MAX == _pool)
		{
			return NULL;
		}
		World::Impl* self = _world.m_impl;
		EcsPool* ptr = self->m_pools[_pool];
		const uint32_t idx = ptr->find(_entity);
		if (UINT32_MAX == idx)
		{
			return NULL;
		}

		if (_mark)
		{
			ptr->m_rows[idx].m_changed = self->m_tick;
		}
		return ptr->at(idx);
	}

	void* EcsAccess::insert(
		  World& _world
		, const EcsType* _type
		, Entity _entity
		, void* _value
		)
	{
		World::Impl* self = _world.m_impl;
		if (!self->writable() || !_world.contains(_entity) )
		{
			return NULL;
		}
		EcsPool* ptr = self->m_pools[pool(_world, _type, true)];
		uint32_t idx = ptr->find(_entity);
		const bool added = UINT32_MAX == idx;
		if (added)
		{
			ptr->grow(_entity.idx);
			idx = ptr->m_rows.m_count;
			const EcsRow value = { _entity, self->m_tick, self->m_tick };
			ptr->m_rows.push(value);
			ptr->m_sparse[_entity.idx] = idx;
		}
		else
		{
			self->callHook(_world, *ptr, _entity, EcsLifecycle::Replace);
			_type->destroy(ptr->at(idx) );
			ptr->m_rows[idx].m_changed = self->m_tick;
		}

		_type->move(ptr->at(idx), _value);
		// Required components are inserted before user hooks and preserve existing values.
		for (uint32_t ii = 0; ii < ptr->m_required.m_count; ++ii)
		{
			const EcsRequired required = ptr->m_required[ii];
			if (NULL == component(_world, required.m_pool, _entity) )
			{
				required.m_insert(_world, _entity);
			}
		}

		if (added)
		{
			self->callHook(_world, *ptr, _entity, EcsLifecycle::Add);
		}

		self->callHook(_world, *ptr, _entity, EcsLifecycle::Insert);
		return ptr->at(idx);
	}

	bool EcsAccess::remove(
		  World& _world
		, const EcsType* _type
		, Entity _entity
		)
	{
		World::Impl* self = _world.m_impl;
		const uint32_t id = pool(_world, _type);
		if (!self->writable() || !_world.contains(_entity) || UINT32_MAX == id)
		{
			return false;
		}
		EcsPool* ptr = self->m_pools[id];
		const uint32_t idx = ptr->find(_entity);
		if (UINT32_MAX == idx)
		{
			return false;
		}

		self->callHook(_world, *ptr, _entity, EcsLifecycle::Replace);
		self->callHook(_world, *ptr, _entity, EcsLifecycle::Remove);
		ptr->erase(idx);
		if (NULL != ptr->m_removed)
		{
			ptr->m_removed(_world, _entity);
		}
		return true;
	}

	void EcsAccess::trackRemoval(
		  World& _world
		, const EcsType* _type
		, EcsRequiredFn _fn
		)
	{
		_world.m_impl->m_pools[pool(_world, _type, true)]->m_removed = _fn;
	}

	bool World::despawn(Entity _entity, bool _recursive)
	{
		World::Impl* self = m_impl;
		if (!self->writable() || !contains(_entity) )
		{
			return false;
		}
		// Iterative postorder traversal avoids a call stack proportional to hierarchy depth.
		Entity cursor = _entity;
		for (;;)
		{
			if (_recursive && contains(self->m_slots[cursor.idx].m_child) )
			{
				cursor = self->m_slots[cursor.idx].m_child;
				continue;
			}
			const Entity parent = getParent(cursor);
			const bool done = cursor == _entity;
			while (contains(getFirstChild(cursor) ) )
			{
				removeParent(getFirstChild(cursor) );
			}

			removeParent(cursor);
			for (uint32_t ii = 0; ii < self->m_pools.m_count; ++ii)
			{
				EcsPool* ptr = self->m_pools[ii];
				if (UINT32_MAX != ptr->find(cursor) )
				{
					self->callHook(*this, *ptr, cursor, EcsLifecycle::Despawn);
					EcsAccess::remove(
						  *this
						, ptr->m_type
						, cursor
						);
				}
			}
			EcsSlot& slot = self->m_slots[cursor.idx];
			slot.m_status = EcsSlot::Status::Free;
			if (UINT32_MAX != slot.m_generation)
			{
				++slot.m_generation;
				slot.m_next = self->m_free;
				self->m_free = cursor.idx;
			}
			--self->m_count;
			for (uint32_t ii = 0; ii < self->m_observers.m_count; ++ii)
			{
				if (self->m_observers[ii].m_target == cursor)
				{
					self->m_observers[ii].m_active = false;
				}
			}

			if (done)
			{
				break;
			}
			cursor = parent;
		}

		self->collectObservers();
		return true;
	}

	bool World::setParent(Entity _child, Entity _parent)
	{
		if (!m_impl->writable() || !contains(_child) || !contains(_parent) )
		{
			return false;
		}

		for (Entity cursor = _parent; contains(cursor); cursor = getParent(cursor) )
		{
			if (cursor == _child)
			{
				return false;
			}
		}

		removeParent(_child);
		EcsSlot& slot = m_impl->m_slots[_child.idx];
		slot.m_parent = _parent;
		slot.m_sibling = m_impl->m_slots[_parent.idx].m_child;
		m_impl->m_slots[_parent.idx].m_child = _child;
		return true;
	}

	bool World::removeParent(Entity _child)
	{
		if (!m_impl->writable() || !contains(_child) )
		{
			return false;
		}
		EcsSlot& slot = m_impl->m_slots[_child.idx];
		if (contains(slot.m_parent) )
		{
			Entity* cursor = &m_impl->m_slots[slot.m_parent.idx].m_child;
			while (contains(*cursor) && *cursor != _child)
			{
				cursor = &m_impl->m_slots[cursor->idx].m_sibling;
			}

			if (*cursor == _child)
			{
				*cursor = slot.m_sibling;
			}
		}
		slot.m_parent = Entity();
		slot.m_sibling = Entity();
		return true;
	}

	Entity World::getParent(Entity _child) const
	{
		return contains(_child) ? m_impl->m_slots[_child.idx].m_parent : Entity();
	}

	Entity World::getFirstChild(Entity _parent) const
	{
		return contains(_parent) ? m_impl->m_slots[_parent.idx].m_child : Entity();
	}

	Entity World::getNextSibling(Entity _child) const
	{
		return contains(_child) ? m_impl->m_slots[_child.idx].m_sibling : Entity();
	}

	bool EcsAccess::changed(
		  World& _world
		, uint32_t _pool
		, Entity _entity
		, uint64_t _since
		, bool _added
		)
	{
		if (NULL == component(_world, _pool, _entity) )
		{
			return false;
		}
		EcsPool* ptr = _world.m_impl->m_pools[_pool];
		const EcsRow& row = ptr->m_rows[ptr->find(_entity)];
		return (_added ? row.m_added : row.m_changed) > _since;
	}

	uint32_t EcsAccess::count(World& _world, uint32_t _pool)
	{
		return UINT32_MAX == _pool ? 0 : _world.m_impl->m_pools[_pool]->m_rows.m_count;
	}

	Entity EcsAccess::at(
		  World& _world
		, uint32_t _pool
		, uint32_t _idx
		)
	{
		return _world.m_impl->m_pools[_pool]->m_rows[_idx].m_entity;
	}

	uint32_t EcsAccess::extent(World& _world)
	{
		return _world.m_impl->m_slots.m_count;
	}

	Entity EcsAccess::at(World& _world, uint32_t _idx)
	{
		return Entity(_idx, _world.m_impl->m_slots[_idx].m_generation);
	}

	bool EcsAccess::enter(World& _world)
	{
		++_world.m_impl->m_iterations;
		return true;
	}

	void EcsAccess::leave(World& _world)
	{
		--_world.m_impl->m_iterations;
	}

	uint64_t EcsAccess::since(World& _world)
	{
		return _world.m_impl->m_inSystem ? _world.m_impl->m_lastRun : _world.m_impl->m_lastClear;
	}

	void World::clearTrackers()
	{
		m_impl->m_lastClear = m_impl->m_tick++;
	}

	void EcsAccess::hook(
		  World& _world
		, const EcsType* _type
		, EcsComponentHookFn _fn
		, void* _userData
		)
	{
		World::Impl* self = _world.m_impl;
		if (self->writable() )
		{
			EcsPool* ptr = self->m_pools[pool(_world, _type, true)];
			ptr->m_hook = _fn;
			ptr->m_userData = _userData;
		}
	}

	bool EcsAccess::required(
		  World& _world
		, const EcsType* _type
		, const EcsType* _required
		, EcsRequiredFn _insert
		)
	{
		World::Impl* self = _world.m_impl;
		if (!self->writable() )
		{
			return false;
		}
		const uint32_t id = pool(_world, _type, true);
		const uint32_t req = pool(_world, _required, true);
		EcsPool* ptr = self->m_pools[id];
		if (0 != ptr->m_rows.m_count || self->reaches(req, id) )
		{
			return false;
		}

		for (uint32_t ii = 0; ii < ptr->m_required.m_count; ++ii)
		{
			if (ptr->m_required[ii].m_pool == req)
			{
				return true;
			}
		}
		const EcsRequired value = { req, _insert };
		ptr->m_required.push(value);
		return true;
	}

	void* EcsAccess::resource(
		  World& _world
		, const EcsType* _type
		, bool _mark
		)
	{
		World::Impl* self = _world.m_impl;
		for (uint32_t ii = 0; ii < self->m_resources.m_count; ++ii)
		{
			EcsValue& value = self->m_resources[ii];
			if (value.m_type == _type)
			{
				if (_mark)
				{
					value.m_changed = self->m_tick;
				}
				return value.m_data;
			}
		}
		return NULL;
	}

	void* EcsAccess::insertResource(
		  World& _world
		, const EcsType* _type
		, void* _value
		)
	{
		World::Impl* self = _world.m_impl;
		FROST_ASSERT(self->writable(), "Insert resources through Commands while iterating.");
		if (!self->writable() )
		{
			return resource(_world, _type);
		}
		void* ptr = ecsMoveValue(self->m_allocator, _type, _value);
		for (uint32_t ii = 0; ii < self->m_resources.m_count; ++ii)
		{
			EcsValue& value = self->m_resources[ii];
			if (value.m_type == _type)
			{
				ecsDropValue(self->m_allocator, _type, value.m_data);
				value.m_data = ptr;
				value.m_changed = self->m_tick;
				return ptr;
			}
		}
		const EcsValue value = { _type, ptr, self->m_tick, self->m_tick };
		self->m_resources.push(value);
		return ptr;
	}

	bool EcsAccess::removeResource(World& _world, const EcsType* _type)
	{
		World::Impl* self = _world.m_impl;
		if (!self->writable() )
		{
			return false;
		}

		for (uint32_t ii = 0; ii < self->m_resources.m_count; ++ii)
		{
			if (self->m_resources[ii].m_type == _type)
			{
				ecsDropValue(self->m_allocator, _type, self->m_resources[ii].m_data);
				self->m_resources[ii] = self->m_resources[self->m_resources.m_count - 1];
				self->m_resources.pop();
				return true;
			}
		}
		return false;
	}

	bool EcsAccess::resourceChanged(
		  World& _world
		, const EcsType* _type
		, uint64_t _since
		, bool _added
		)
	{
		World::Impl* self = _world.m_impl;
		for (uint32_t ii = 0; ii < self->m_resources.m_count; ++ii)
		{
			const EcsValue& value = self->m_resources[ii];
			if (value.m_type == _type)
			{
				return (_added ? value.m_added : value.m_changed) > _since;
			}
		}
		return false;
	}

	void EcsAccess::queue(
		  World& _world
		, const EcsType* _type
		, void* _value
		, EcsCommandRunFn _run
		)
	{
		World::Impl* self = _world.m_impl;
		EcsCommand* cmd = FROST_NEW(self->m_allocator, EcsCommand)();
		cmd->m_next = NULL;
		cmd->m_type = _type;
		cmd->m_data = ecsMoveValue(self->m_allocator, _type, _value);
		cmd->m_run = _run;
		if (NULL == self->m_head)
		{
			self->m_head = cmd;
		}
		else
		{
			self->m_tail->m_next = cmd;
		}
		self->m_tail = cmd;
	}

	bool World::flush()
	{
		World::Impl* self = m_impl;
		if (!self->writable() || self->m_flushing)
		{
			return false;
		}
		self->m_flushing = true;
		while (NULL != self->m_head)
		{
			EcsCommand* cmd = self->m_head;
			self->m_head = cmd->m_next;
			if (NULL == self->m_head)
			{
				self->m_tail = NULL;
			}

			cmd->m_run(*this, cmd->m_data);
			ecsDropValue(self->m_allocator, cmd->m_type, cmd->m_data);
			frost::deleteObject(self->m_allocator, cmd);
		}
		self->m_flushing = false;
		return true;
	}

	void EcsAccess::message(
		  World& _world
		, const EcsType* _type
		, void* _value
		)
	{
		World::Impl* self = _world.m_impl;
		const EcsMessage value = { _type,
			ecsMoveValue(self->m_allocator, _type, _value), ++self->m_sequence, self->m_frame };
		self->m_messages.push(value);
	}

	void World::updateMessages()
	{
		World::Impl* self = m_impl;
		if (!self->writable() )
		{
			return;
		}
		++self->m_frame;
		uint32_t dst = 0;
		for (uint32_t ii = 0; ii < self->m_messages.m_count; ++ii)
		{
			const EcsMessage msg = self->m_messages[ii];
			if (self->m_frame - msg.m_frame >= 2)
			{
				ecsDropValue(self->m_allocator, msg.m_type, msg.m_data);
			}
			else
			{
				self->m_messages[dst++] = msg;
			}
		}

		while (self->m_messages.m_count > dst)
		{
			self->m_messages.pop();
		}
	}

	void EcsAccess::read(
		  World& _world
		, const EcsType* _type
		, uint64_t& _sequence
		, EcsReadFn _fn
		, void* _userData
		)
	{
		World::Impl* self = _world.m_impl;
		EcsIteration iteration(_world);
		const uint32_t count = self->m_messages.m_count;
		const uint64_t previous = _sequence;
		_sequence = self->m_sequence;
		for (uint32_t ii = 0; ii < count; ++ii)
		{
			// Copy metadata because callbacks may append messages and grow the array.
			const EcsMessage msg = self->m_messages[ii];
			if (msg.m_type == _type && msg.m_sequence > previous)
			{
				_fn(msg.m_data, _userData);
			}
		}
	}

	uint32_t EcsAccess::observe(
		  World& _world
		, const EcsType* _event
		, const EcsType* _type
		, void* _value
		, EcsObserverFn _fn
		, Entity _target
		)
	{
		World::Impl* self = _world.m_impl;
		const uint32_t id = self->m_observers.m_count;
		const EcsObserver value = { _event, _type,
			ecsMoveValue(self->m_allocator, _type, _value), _fn, _target, true };
		self->m_observers.push(value);
		return id;
	}

	bool World::removeObserver(uint32_t _id)
	{
		if (_id >= m_impl->m_observers.m_count || !m_impl->m_observers[_id].m_active)
		{
			return false;
		}
		m_impl->m_observers[_id].m_active = false;
		m_impl->collectObservers();
		return true;
	}

	void EcsAccess::trigger(
		  World& _world
		, const EcsType* _type
		, const void* _event
		, Entity _target
		)
	{
		World::Impl* self = _world.m_impl;
		const uint32_t count = self->m_observers.m_count;
		++self->m_dispatch;
		for (uint32_t ii = 0; ii < count; ++ii)
		{
			const EcsObserver obs = self->m_observers[ii];
			if (obs.m_active && obs.m_event == _type
			&& (UINT32_MAX == obs.m_target.idx || obs.m_target == _target) )
			{
				obs.m_run(_world, _target, _event, obs.m_data);
			}
		}
		--self->m_dispatch;
		self->collectObservers();
	}

	void* EcsAccess::local(
		  World& _world
		, void*& _locals
		, const EcsType* _type
		, EcsInitFn _init
		)
	{
		EcsLocal* ptr = (EcsLocal*)_locals;
		while (NULL != ptr)
		{
			if (ptr->m_type == _type)
			{
				return ptr->m_data;
			}
			ptr = ptr->m_next;
		}
		AllocatorI* allocator = _world.m_impl->m_systemAllocator;
		FROST_ASSERT(NULL != allocator, "LocalT is available only within a running system.");
		ptr = FROST_NEW(allocator, EcsLocal)();
		ptr->m_type = _type;
		ptr->m_next = (EcsLocal*)_locals;
		ptr->m_data = frost::alloc(allocator, _type->size, _type->align);
		_init(ptr->m_data);
		_locals = ptr;
		return ptr->m_data;
	}

	struct Schedule::Impl
	{
		explicit Impl(AllocatorI* _allocator)
			: m_allocator(_allocator)
			, m_systems(_allocator)
			, m_sets(_allocator)
			, m_edges(_allocator)
			, m_order(_allocator)
			, m_incoming(_allocator)
			, m_matrix(_allocator)
			, m_world(NULL)
			, m_dirty(true)
			, m_cycle(false)
			, m_running(false)
		{
		}

		~Impl()
		{
			for (uint32_t ii = 0; ii < m_systems.m_count; ++ii)
			{
				ecsDropValue(m_allocator, m_systems[ii].m_type, m_systems[ii].m_data);
				ecsDropLocals(m_allocator, m_systems[ii].m_locals);
			}
		}

		EcsSet& set(uint32_t _id)
		{
			for (uint32_t ii = 0; ii < m_sets.m_count; ++ii)
			{
				if (m_sets[ii].m_id == _id)
				{
					return m_sets[ii];
				}
			}

			m_sets.push(EcsSet(_id));
			return m_sets[m_sets.m_count - 1];
		}

		bool compile()
		{
			if (!m_dirty)
			{
				return !m_cycle;
			}
			m_dirty = false;
			m_cycle = false;
			m_order.clear();
			m_incoming.clear();
			m_matrix.clear();
			const uint32_t count = m_systems.m_count;
			// A byte adjacency matrix makes edge deduplication and cycle detection deterministic.
			FROST_ASSERT(uint64_t(count)*count <= UINT32_MAX, "Too many ECS systems.");
			for (uint32_t ii = 0; ii < count*count; ++ii)
			{
				m_matrix.push(0);
			}

			for (uint32_t ee = 0; ee < m_edges.m_count; ++ee)
			{
				const EcsEdge edge = m_edges[ee];
				for (uint32_t from = 0; from < count; ++from)
				{
					if (edge.m_from != (edge.m_fromSet ? m_systems[from].m_set : from) )
					{
						continue;
					}

					for (uint32_t to = 0; to < count; ++to)
					{
						if (edge.m_to == (edge.m_toSet ? m_systems[to].m_set : to) )
						{
							m_matrix[from*count + to] = 1;
						}
					}
				}
			}

			EcsArrayT<uint32_t> degree(m_allocator);
			for (uint32_t to = 0; to < count; ++to)
			{
				uint32_t num = 0;
				for (uint32_t from = 0; from < count; ++from)
				{
					num += m_matrix[from*count + to];
				}

				degree.push(num);
				m_incoming.push(0 != num ? 1 : 0);
			}

			while (m_order.m_count < count)
			{
				uint32_t next = 0;
				while (next < count && 0 != degree[next])
				{
					++next;
				}

				if (next == count)
				{
					m_cycle = true;
					return false;
				}

				m_order.push(next);
				degree[next] = UINT32_MAX;
				for (uint32_t to = 0; to < count; ++to)
				{
					if (0 != m_matrix[next*count + to])
					{
						--degree[to];
					}
				}
			}
			return true;
		}

		AllocatorI*          m_allocator;
		EcsArrayT<EcsSystem> m_systems;
		EcsArrayT<EcsSet>    m_sets;
		EcsArrayT<EcsEdge>   m_edges;
		EcsArrayT<uint32_t>  m_order;
		EcsArrayT<uint8_t>   m_incoming;
		EcsArrayT<uint8_t>   m_matrix;
		World*               m_world;
		bool                 m_dirty;
		bool                 m_cycle;
		bool                 m_running;
	};

	Schedule::Schedule(AllocatorI* _allocator)
		: m_impl(FROST_NEW(_allocator, Impl)(_allocator) )
	{
	}

	Schedule::~Schedule()
	{
		frost::deleteObject(m_impl->m_allocator, m_impl);
	}

	uint32_t Schedule::getSystemCount() const
	{
		return m_impl->m_systems.m_count;
	}

	EcsScheduleResult::Enum Schedule::run(World& _world)
	{
		return EcsAccess::runSchedule(*this, _world);
	}

	SystemConfig Schedule::configureSet(uint32_t _set)
	{
		FROST_ASSERT(!m_impl->m_running, "Cannot configure a running ECS schedule.");
		if (!m_impl->m_running)
		{
			m_impl->set(_set);
		}

		return SystemConfig(*this, _set, true);
	}

	SystemConfig EcsAccess::system(
		  Schedule& _schedule
		, const EcsType* _type
		, void* _value
		, EcsSystemFn _run
		, EcsSystemReadyFn _ready
		, bool _exclusive
		)
	{
		Schedule::Impl* self = _schedule.m_impl;
		FROST_ASSERT(!self->m_running, "Cannot add systems to a running ECS schedule.");
		if (self->m_running)
		{
			return SystemConfig(_schedule, UINT32_MAX);
		}
		const uint32_t idx = self->m_systems.m_count;
		EcsSystem system;
		system.m_type = _type;
		system.m_data = ecsMoveValue(self->m_allocator, _type, _value);
		system.m_run = _run;
		system.m_ready = _ready;
		system.m_exclusive = _exclusive;
		self->m_systems.push(system);
		self->m_dirty = true;
		return SystemConfig(_schedule, idx);
	}

	SystemConfig::SystemConfig(
		  Schedule& _schedule
		, uint32_t _idx
		, bool _set
		)
		: m_schedule(&_schedule)
		, m_idx(_idx)
		, m_set(_set)
	{
	}

	SystemId SystemConfig::id() const
	{
		return SystemId(m_set ? UINT32_MAX : m_idx);
	}

	SystemConfig& SystemConfig::before(SystemId _other)
	{
		Schedule::Impl* self = m_schedule->m_impl;
		if (!self->m_running && _other.idx < self->m_systems.m_count)
		{
			const EcsEdge value = { m_idx, _other.idx, m_set, false };
			self->m_edges.push(value);
			self->m_dirty = true;
		}
		return *this;
	}

	SystemConfig& SystemConfig::after(SystemId _other)
	{
		Schedule::Impl* self = m_schedule->m_impl;
		if (!self->m_running && _other.idx < self->m_systems.m_count)
		{
			const EcsEdge value = { _other.idx, m_idx, false, m_set };
			self->m_edges.push(value);
			self->m_dirty = true;
		}
		return *this;
	}

	SystemConfig& SystemConfig::beforeSet(uint32_t _set)
	{
		Schedule::Impl* self = m_schedule->m_impl;
		if (!self->m_running)
		{
			const EcsEdge value = { m_idx, _set, m_set, true };
			self->m_edges.push(value);
			self->m_dirty = true;
		}
		return *this;
	}

	SystemConfig& SystemConfig::afterSet(uint32_t _set)
	{
		Schedule::Impl* self = m_schedule->m_impl;
		if (!self->m_running)
		{
			const EcsEdge value = { _set, m_idx, true, m_set };
			self->m_edges.push(value);
			self->m_dirty = true;
		}
		return *this;
	}

	SystemConfig& SystemConfig::inSet(uint32_t _set)
	{
		Schedule::Impl* self = m_schedule->m_impl;
		if (!self->m_running && !m_set && m_idx < self->m_systems.m_count)
		{
			self->m_systems[m_idx].m_set = _set;
			self->set(_set);
			self->m_dirty = true;
		}
		return *this;
	}

	SystemConfig& SystemConfig::runIf(EcsRunConditionFn _fn, void* _userData)
	{
		Schedule::Impl* self = m_schedule->m_impl;
		if (!self->m_running)
		{
			if (m_set)
			{
				EcsSet& set = self->set(m_idx);
				set.m_condition = _fn;
				set.m_userData = _userData;
			}

			else if (m_idx < self->m_systems.m_count)
			{
				self->m_systems[m_idx].m_condition = _fn;
				self->m_systems[m_idx].m_userData = _userData;
			}
		}
		return *this;
	}

	EcsScheduleResult::Enum EcsAccess::runSchedule(Schedule& _schedule, World& _world)
	{
		Schedule::Impl* self = _schedule.m_impl;
		World::Impl* world = _world.m_impl;
		if (self->m_running || !world->writable() )
		{
			return EcsScheduleResult::Busy;
		}

		if (NULL != self->m_world && self->m_world != &_world)
		{
			return EcsScheduleResult::WorldMismatch;
		}

		if (!self->compile() )
		{
			return EcsScheduleResult::Cycle;
		}
		self->m_world = &_world;
		self->m_running = true;
		const bool wasInSystem = world->m_inSystem;
		const uint64_t lastRun = world->m_lastRun;
		AllocatorI* allocator = world->m_systemAllocator;
		EcsScheduleResult::Enum result = EcsScheduleResult::Success;
		for (uint32_t ii = 0; ii < self->m_order.m_count; ++ii)
		{
			const uint32_t idx = self->m_order[ii];
			EcsSystem& system = self->m_systems[idx];
			if (system.m_exclusive || 0 != self->m_incoming[idx])
			{
				_world.flush();
			}
			world->m_lastRun = system.m_lastRun;
			world->m_inSystem = true;
			world->m_systemAllocator = self->m_allocator;
			bool enabled = true;
			if (UINT32_MAX != system.m_set)
			{
				EcsSet& set = self->set(system.m_set);
				enabled = NULL == set.m_condition || set.m_condition(_world, set.m_userData);
			}

			if (!enabled || (NULL != system.m_condition && !system.m_condition(_world, system.m_userData) ) )
			{
				continue;
			}

			if (!system.m_ready(_world) )
			{
				result = EcsScheduleResult::MissingResource;
				break;
			}
			const uint64_t tick = ++world->m_tick;
			system.m_run(_world, system.m_data, system.m_locals);
			system.m_lastRun = tick;
			// Deferred writes occur after this system's change window.
			++world->m_tick;
		}
		world->m_lastRun = lastRun;
		world->m_inSystem = wasInSystem;
		world->m_systemAllocator = allocator;
		_world.flush();
		self->m_running = false;
		return result;
	}

	void Commands::queue(EcsCommandFn _fn)
	{
		queue(EcsFunctionCommand(_fn) );
	}

	PluginI::~PluginI()
	{
	}

	bool PluginI::ready(const App&) const
	{
		return true;
	}

	void PluginI::finish(App&)
	{
	}

	void PluginI::cleanup(App&)
	{
	}

	bool PluginI::isUnique() const
	{
		return true;
	}

	const char* PluginI::getName() const
	{
		return NULL;
	}

	struct App::Impl
	{
		explicit Impl(AllocatorI* _allocator)
			: m_allocator(_allocator)
			, m_world(_allocator)
			, m_schedules(_allocator)
			, m_plugins(_allocator)
			, m_states(_allocator)
			, m_runner(NULL)
			, m_runnerData(NULL)
			, m_building(0)
			, m_finished(false)
			, m_started(false)
			, m_startupAttempted(false)
			, m_shutdown(false)
			, m_shutdownResult(EcsScheduleResult::Busy)
			, m_shutdownStage(EcsStage::PreShutdown)
			, m_updating(false)
			, m_exit(false)
		{
			for (uint32_t ii = 0; ii < uint32_t(EcsStage::Count); ++ii)
			{
				m_stages[ii] = FROST_NEW(m_allocator, Schedule)(m_allocator);
			}

			m_world.insertResource<TimeResource>();
		}

		~Impl()
		{
			for (uint32_t ii = 0; ii < m_states.m_count; ++ii)
			{
				ecsDropValue(m_allocator, m_states[ii].m_type, m_states[ii].m_data);
			}

			for (uint32_t ii = 0; ii < m_plugins.m_count; ++ii)
			{
				ecsDropValue(m_allocator, m_plugins[ii].m_type, m_plugins[ii].m_allocation);
			}

			for (uint32_t ii = 0; ii < m_schedules.m_count; ++ii)
			{
				const EcsAppSchedule& entry = m_schedules[ii];
				frost::deleteObject(m_allocator, entry.m_schedule);
				if (NULL != entry.m_state)
				{
					ecsDropValue(m_allocator, entry.m_label, entry.m_state);
				}
			}

			for (uint32_t ii = 0; ii < uint32_t(EcsStage::Count); ++ii)
			{
				frost::deleteObject(m_allocator, m_stages[ii]);
			}
		}

		AllocatorI*               m_allocator;
		World                     m_world;
		Schedule*                 m_stages[uint32_t(EcsStage::Count)];
		EcsArrayT<EcsAppSchedule> m_schedules;
		EcsArrayT<EcsPlugin>      m_plugins;
		EcsArrayT<EcsStateDriver> m_states;
		EcsRunnerFn               m_runner;
		void*                     m_runnerData;
		uint32_t                  m_building;
		bool                      m_finished;
		bool                      m_started;
		bool                      m_startupAttempted;
		bool                      m_shutdown;
		EcsScheduleResult::Enum    m_shutdownResult;
		uint32_t                  m_shutdownStage;
		bool                      m_updating;
		bool                      m_exit;
	};

	App::App(AllocatorI* _allocator)
		: m_impl(FROST_NEW(_allocator, Impl)(_allocator) )
	{
	}

	App::~App()
	{
		shutdown();
		frost::deleteObject(m_impl->m_allocator, m_impl);
	}

	World& App::getWorld()
	{
		return m_impl->m_world;
	}

	const World& App::getWorld() const
	{
		return m_impl->m_world;
	}

	Schedule& App::getSchedule(EcsStage::Enum _stage)
	{
		FROST_ASSERT(0 <= _stage && _stage < EcsStage::Count, "Invalid ECS schedule stage.");
		return *m_impl->m_stages[uint32_t(_stage)];
	}

	Schedule& EcsAccess::schedule(App& _app, const EcsType* _label)
	{
		App::Impl* self = _app.m_impl;
		for (uint32_t ii = 0; ii < self->m_schedules.m_count; ++ii)
		{
			EcsAppSchedule& entry = self->m_schedules[ii];
			if (entry.m_label == _label && NULL == entry.m_state)
			{
				return *entry.m_schedule;
			}
		}
		Schedule* ptr = FROST_NEW(self->m_allocator, Schedule)(self->m_allocator);
		self->m_schedules.push(EcsAppSchedule(_label, ptr));
		return *ptr;
	}

	Schedule& EcsAccess::stateSchedule(
		  App& _app
		, const EcsType* _type
		, void* _value
		, bool _enter
		, EcsStateEqualFn _equal
		)
	{
		App::Impl* self = _app.m_impl;
		for (uint32_t ii = 0; ii < self->m_schedules.m_count; ++ii)
		{
			EcsAppSchedule& entry = self->m_schedules[ii];
			if (entry.m_label == _type && NULL != entry.m_state && entry.m_enter == _enter
			&& entry.m_equal(entry.m_state, _value) )
			{
				return *entry.m_schedule;
			}
		}
		Schedule* ptr = FROST_NEW(self->m_allocator, Schedule)(self->m_allocator);
		self->m_schedules.push(EcsAppSchedule(_type, ptr,
			ecsMoveValue(self->m_allocator, _type, _value), _enter, _equal));
		return *ptr;
	}

	bool EcsAccess::runState(
		  App& _app
		, const EcsType* _type
		, const void* _value
		, bool _enter
		)
	{
		App::Impl* self = _app.m_impl;
		for (uint32_t ii = 0; ii < self->m_schedules.m_count; ++ii)
		{
			const EcsAppSchedule entry = self->m_schedules[ii];
			if (entry.m_label == _type && NULL != entry.m_state && entry.m_enter == _enter
			&& entry.m_equal(entry.m_state, _value) )
			{
				return EcsScheduleResult::Success == entry.m_schedule->run(self->m_world);
			}
		}
		return true;
	}

	void EcsAccess::state(
		  App& _app
		, const EcsType* _type
		, void* _value
		, EcsStateFn _run
		)
	{
		App::Impl* self = _app.m_impl;
		const EcsStateDriver value = { _type,
			ecsMoveValue(self->m_allocator, _type, _value), _run };
		self->m_states.push(value);
	}

	bool EcsAccess::hasPlugin(const App& _app, const EcsType* _type)
	{
		App::Impl* const self = _app.m_impl;
		for (uint32_t ii = 0; ii < self->m_plugins.m_count; ++ii)
		{
			if (self->m_plugins[ii].m_type == _type)
			{
				return true;
			}
		}
		return false;
	}

	bool EcsAccess::plugin(
		  App& _app
		, PluginI* _plugin
		, const EcsType* _type
		, void* _allocation
		)
	{
		App::Impl* self = _app.m_impl;
		bool duplicate = false;
		const char* name = _plugin->getName();
		if (_plugin->isUnique() )
		{
			for (uint32_t ii = 0; ii < self->m_plugins.m_count; ++ii)
			{
				const EcsPlugin& other = self->m_plugins[ii];
				const char* otherName = other.m_plugin->getName();
				duplicate |= NULL == name ? other.m_type == _type
					: (NULL != otherName && 0 == strCmp(name, otherName) );
			}
		}
		// PluginI can be a base subobject, so retain the complete allocation for destruction.
		if (self->m_finished || self->m_shutdown || duplicate)
		{
			ecsDropValue(self->m_allocator, _type, _allocation);
			return false;
		}
		const EcsPlugin value = { _type, _plugin, _allocation };
		self->m_plugins.push(value);
		++self->m_building;
		_plugin->build(_app);
		--self->m_building;
		return true;
	}

	static bool ecsRunStage(App& _app, EcsStage::Enum _stage)
	{
		return EcsScheduleResult::Success == _app.getSchedule(_stage).run(_app.getWorld() );
	}

	bool App::update(double _delta)
	{
		App::Impl* self = m_impl;
		if (self->m_updating || self->m_exit || 0 != self->m_building
		|| !(_delta >= 0.0) || !isFinite(_delta) )
		{
			return false;
		}
		self->m_updating = true;
		if (!self->m_finished)
		{
			for (uint32_t ii = 0; ii < self->m_plugins.m_count; ++ii)
			{
				if (!self->m_plugins[ii].m_plugin->ready(*this) )
				{
					self->m_updating = false;
					return false;
				}
			}
			self->m_finished = true;
			for (uint32_t ii = 0; ii < self->m_plugins.m_count; ++ii)
			{
				self->m_plugins[ii].m_plugin->finish(*this);
			}

			for (uint32_t ii = 0; ii < self->m_plugins.m_count; ++ii)
			{
				self->m_plugins[ii].m_plugin->cleanup(*this);
			}
		}
		bool success = true;
		if (!self->m_started)
		{
			self->m_startupAttempted = true;
			success = ecsRunStage(*this, EcsStage::PreStartup) && ecsRunStage(*this, EcsStage::Startup) && ecsRunStage(*this, EcsStage::PostStartup);
			self->m_started = success;
		}

		if (!success)
		{
			self->m_updating = false;
			return false;
		}

		self->m_world.updateMessages();
		TimeResource time = *self->m_world.getResource<TimeResource>();
		time.delta = _delta;
		time.elapsed += _delta;
		time.accumulator += _delta;
		++time.frame;
		*self->m_world.getResourceMut<TimeResource>() = time;
		success = ecsRunStage(*this, EcsStage::First) && ecsRunStage(*this, EcsStage::PreUpdate);
		for (uint32_t ii = 0; success && ii < self->m_states.m_count; ++ii)
		{
			const EcsStateDriver state = self->m_states[ii];
			success = state.m_run(*this, state.m_data);
		}
		time = *self->m_world.getResource<TimeResource>();
		// Reject zero, negative, NaN, and infinite fixed intervals before entering the loop.
		if (!(time.fixedDelta > 0.0) || !isFinite(time.fixedDelta) || !isFinite(time.accumulator) )
		{
			success = false;
		}

		while (success && time.accumulator >= time.fixedDelta)
		{
			time.accumulator -= time.fixedDelta;
			time.fixedElapsed += time.fixedDelta;
			time.delta = time.fixedDelta;
			*self->m_world.getResourceMut<TimeResource>() = time;
			success = ecsRunStage(*this, EcsStage::FixedFirst) && ecsRunStage(*this, EcsStage::FixedPreUpdate)
				&& ecsRunStage(*this, EcsStage::FixedUpdate) && ecsRunStage(*this, EcsStage::FixedPostUpdate) && ecsRunStage(
					  *this
					, EcsStage::FixedLast
					);
			time = *self->m_world.getResource<TimeResource>();
			if (!(time.fixedDelta > 0.0) || !isFinite(time.fixedDelta) || !isFinite(time.accumulator) )
			{
				success = false;
			}
		}
		time.delta = _delta;
		*self->m_world.getResourceMut<TimeResource>() = time;
		success = success && ecsRunStage(*this, EcsStage::Update) && ecsRunStage(*this, EcsStage::PostUpdate) && ecsRunStage(*this, EcsStage::Last);
		self->m_world.clearTrackers();
		self->m_updating = false;
		return success;
	}

	void App::setRunner(EcsRunnerFn _fn, void* _userData)
	{
		m_impl->m_runner = _fn;
		m_impl->m_runnerData = _userData;
	}

	void App::run()
	{
		if (m_impl->m_exit)
		{
			shutdown();
			return;
		}

		if (NULL != m_impl->m_runner)
		{
			m_impl->m_runner(*this, m_impl->m_runnerData);
		}
		else
		{
			update();
		}
		shutdown();
	}

	bool App::shutdown()
	{
		Impl* self = m_impl;
		if (self->m_shutdown)
		{
			return EcsScheduleResult::Success == self->m_shutdownResult;
		}
		if (self->m_updating || 0 != self->m_building)
		{
			return false;
		}

		self->m_exit = true;
		// Mark before invoking systems to prevent recursive shutdown.
		self->m_shutdown = true;
		self->m_shutdownResult = EcsScheduleResult::Success;
		if (self->m_startupAttempted)
		{
			for (; self->m_shutdownStage <= EcsStage::PostShutdown; ++self->m_shutdownStage)
			{
				self->m_shutdownResult = getSchedule(EcsStage::Enum(self->m_shutdownStage)).run(self->m_world);
				if (EcsScheduleResult::Success != self->m_shutdownResult)
				{
					break;
				}
			}
		}
		if (EcsScheduleResult::Busy == self->m_shutdownResult)
		{
			// Retry the current phase without repeating completed teardown.
			self->m_shutdown = false;
		}
		return EcsScheduleResult::Success == self->m_shutdownResult;
	}

	void App::exit()
	{
		m_impl->m_exit = true;
	}

	bool App::shouldExit() const
	{
		return m_impl->m_exit;
	}

} // namespace frost
