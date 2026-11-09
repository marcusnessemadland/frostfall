/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_ECS_H_HEADER_GUARD
#	error "Must be included from frost/ecs.h"
#endif // FROST_ECS_H_HEADER_GUARD

namespace frost
{

	typedef void (*EcsRequiredFn)(World&, Entity);
	typedef void (*EcsReadFn)(const void*, void*);
	typedef void (*EcsObserverFn)(World&, Entity, const void*, void*);
	typedef void (*EcsSystemFn)(World&, void*, void*&);
	typedef bool (*EcsSystemReadyFn)(World&);
	typedef void (*EcsCommandRunFn)(World&, void*);
	typedef void (*EcsInitFn)(void*);
	typedef bool (*EcsStateFn)(App&, void*);
	typedef bool (*EcsStateEqualFn)(const void*, const void*);

	/// Implementation bridge. IDs and pointers returned here are World local.
	///
	struct EcsAccess
	{
		static uint32_t pool(
			  World& _world
			, const EcsType* _type
			, bool _create = false
			);
		static void* component(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, bool _mark = false
			);
		static void* insert(
			  World& _world
			, const EcsType* _type
			, Entity _entity
			, void* _value
			);
		static bool remove(
			  World& _world
			, const EcsType* _type
			, Entity _entity
			);
		static bool changed(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			, bool _added
			);
		static uint32_t count(World& _world, uint32_t _pool);
		static Entity at(
			  World& _world
			, uint32_t _pool
			, uint32_t _idx
			);
		static uint32_t extent(World& _world);
		static Entity at(World& _world, uint32_t _idx);
		static Entity reserve(World& _world);
		static Entity create(World& _world);
		static bool activate(World& _world, Entity _entity);
		static bool enter(World& _world);
		static void leave(World& _world);
		static uint64_t since(World& _world);
		static void hook(
			  World& _world
			, const EcsType* _type
			, EcsComponentHookFn _fn
			, void* _userData
			);
		static bool required(
			  World& _world
			, const EcsType* _type
			, const EcsType* _required
			, EcsRequiredFn _insert
			);
		static void trackRemoval(
			  World& _world
			, const EcsType* _type
			, EcsRequiredFn _fn
			);
		static void* resource(
			  World& _world
			, const EcsType* _type
			, bool _mark = false
			);
		static void* insertResource(
			  World& _world
			, const EcsType* _type
			, void* _value
			);
		static bool removeResource(World& _world, const EcsType* _type);
		static bool resourceChanged(
			  World& _world
			, const EcsType* _type
			, uint64_t _since
			, bool _added
			);
		static void queue(
			  World& _world
			, const EcsType* _type
			, void* _value
			, EcsCommandRunFn _run
			);
		static void message(
			  World& _world
			, const EcsType* _type
			, void* _value
			);
		static void read(
			  World& _world
			, const EcsType* _type
			, uint64_t& _sequence
			, EcsReadFn _fn
			, void* _userData
			);
		static uint32_t observe(
			  World& _world
			, const EcsType* _event
			, const EcsType* _type
			, void* _value
			, EcsObserverFn _fn
			, Entity _target
			);
		static void trigger(
			  World& _world
			, const EcsType* _type
			, const void* _event
			, Entity _target
			);
		static void* local(
			  World& _world
			, void*& _locals
			, const EcsType* _type
			, EcsInitFn _init
			);
		static SystemConfig system(
			  Schedule& _schedule
			, const EcsType* _type
			, void* _value
			, EcsSystemFn _run
			, EcsSystemReadyFn _ready
			, bool _exclusive
			);
		static EcsScheduleResult::Enum runSchedule(Schedule& _schedule, World& _world);
		static bool plugin(
			  App& _app
			, PluginI* _plugin
			, const EcsType* _type
			, void* _allocation
			);
		static bool hasPlugin(const App& _app, const EcsType* _type);
		static Schedule& schedule(App& _app, const EcsType* _label);
		static Schedule& stateSchedule(
			  App& _app
			, const EcsType* _type
			, void* _value
			, bool _enter
			, EcsStateEqualFn _equal
			);
		static bool runState(
			  App& _app
			, const EcsType* _type
			, const void* _value
			, bool _enter
			);
		static void state(
			  App& _app
			, const EcsType* _type
			, void* _value
			, EcsStateFn _run
			);
	};

	template<typename Ty, bool MoveT = isConstructible<Ty, Ty&&>() >
	struct EcsMoveT
	{
		static void move(void* _dst, void* _src)
		{
			FROST_PLACEMENT_NEW(_dst, Ty)(frost::move(*(Ty*)_src) );
		}
	};

	template<typename Ty>
	struct EcsMoveT<Ty, false>
	{
		static void move(void* _dst, void* _src)
		{
			FROST_UNUSED(_dst, _src);
			FROST_ASSERT(false, "ECS values must be move constructible.");
		}
	};

	template<typename Ty>
	struct EcsTypeT : public EcsMoveT<Ty>
	{
		static void destroy(void* _ptr)
		{
			((Ty*)_ptr)->~Ty();
		}

		static void init(void* _ptr)
		{
			FROST_PLACEMENT_NEW(_ptr, Ty)();
		}
	};

	template<typename Ty>
	struct EcsValueT
	{
		typedef RemoveCvType<RemoveReferenceType<Ty> > Type;
	};

	template<typename Ty>
	struct EcsTypeIdT
	{
		static const EcsType* get()
		{
			static const EcsType type =
			{
				sizeof(Ty),
				FROST_ALIGNOF(Ty),
				&EcsTypeT<Ty>::move,
				&EcsTypeT<Ty>::destroy
			};

			return &type;
		}
	};

	struct EcsIteration
	{
		explicit EcsIteration(World& _world)
			: m_world(_world)
		{
			EcsAccess::enter(m_world);
		}

		~EcsIteration()
		{
			EcsAccess::leave(m_world);
		}

		World& m_world;
	};

	template<typename Ty>
	struct EcsTermT
	{
		static_assert(isLvalueReference<Ty>(), "Query data must be a reference, Entity, or OptionalT.");
		typedef typename EcsValueT<Ty>::Type ValueType;
		static const bool kFilter = false;
		static const bool kRequired = true;
		static uint32_t prepare(World& _world)
		{
			return EcsAccess::pool(_world, getEcsType<ValueType>(), true);
		}

		static bool matches(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t
			)
		{
			return NULL != EcsAccess::component(_world, _pool, _entity);
		}

		static Ty get(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t
			)
		{
			return *(ValueType*)EcsAccess::component(_world, _pool, _entity,
				!isConst<RemoveReferenceType<Ty> >() );
		}
	};

	template<>
	struct EcsTermT<Entity>
	{
		static const bool kFilter = false;
		static const bool kRequired = false;
		static uint32_t prepare(World&)
		{
			return UINT32_MAX;
		}

		static bool matches(
			  World&
			, uint32_t
			, Entity
			, uint64_t
			)
		{
			return true;
		}

		static Entity get(
			  World&
			, uint32_t
			, Entity _entity
			, uint64_t
			)
		{
			return _entity;
		}
	};

	template<typename Ty>
	struct EcsTermT<OptionalT<Ty> >
	{
		static const bool kFilter = false;
		static const bool kRequired = false;
		static uint32_t prepare(World& _world)
		{
			return EcsAccess::pool(_world, getEcsType<Ty>(), true);
		}

		static bool matches(
			  World&
			, uint32_t
			, Entity
			, uint64_t
			)
		{
			return true;
		}

		static Ty* get(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t
			)
		{
			return (Ty*)EcsAccess::component(_world, _pool, _entity, !isConst<Ty>() );
		}
	};

	template<typename Ty>
	struct EcsTermT<RefT<Ty> >
	{
		static const bool kFilter = false;
		static const bool kRequired = true;
		static uint32_t prepare(World& _world)
		{
			return EcsAccess::pool(_world, getEcsType<Ty>(), true);
		}

		static bool matches(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t
			)
		{
			return NULL != EcsAccess::component(_world, _pool, _entity);
		}

		static RefT<Ty> get(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			)
		{
			return RefT<Ty>(_world, _pool, _entity, _since);
		}
	};

	template<typename Ty>
	struct EcsTermT<MutT<Ty> > : EcsTermT<RefT<Ty> >
	{
		static MutT<Ty> get(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			)
		{
			return MutT<Ty>(_world, _pool, _entity, _since);
		}
	};

	template<typename Ty>
	struct EcsTermT<WithT<Ty> >
	{
		static const bool kFilter = true;
		static const bool kRequired = true;
		static uint32_t prepare(World& _world)
		{
			return EcsAccess::pool(_world, getEcsType<Ty>(), true);
		}

		static bool matches(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t
			)
		{
			return NULL != EcsAccess::component(_world, _pool, _entity);
		}
	};

	template<typename Ty>
	struct EcsTermT<WithoutT<Ty> > : EcsTermT<WithT<Ty> >
	{
		static const bool kRequired = false;
		static bool matches(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			)
		{
			return !EcsTermT<WithT<Ty> >::matches(_world, _pool, _entity, _since);
		}
	};

	template<typename Ty>
	struct EcsTermT<AddedT<Ty> > : EcsTermT<WithT<Ty> >
	{
		static bool matches(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			)
		{
			return EcsAccess::changed(_world, _pool, _entity, _since, true);
		}
	};

	template<typename Ty>
	struct EcsTermT<ChangedT<Ty> > : EcsTermT<WithT<Ty> >
	{
		static bool matches(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			)
		{
			return EcsAccess::changed(_world, _pool, _entity, _since, false);
		}
	};

	template<typename... TermsT>
	struct EcsOrT;

	template<>
	struct EcsOrT<>
	{
		static void prepare(World&)
		{
		}

		static bool matches(
			  World&
			, Entity
			, uint64_t
			)
		{
			return false;
		}
	};

	template<typename TermT, typename... TermsT>
	struct EcsOrT<TermT, TermsT...>
	{
		static void prepare(World& _world)
		{
			EcsTermT<TermT>::prepare(_world);
			EcsOrT<TermsT...>::prepare(_world);
		}

		static bool matches(
			  World& _world
			, Entity _entity
			, uint64_t _since
			)
		{
			return EcsTermT<TermT>::matches(_world,
				EcsTermT<TermT>::prepare(_world), _entity, _since)
				|| EcsOrT<TermsT...>::matches(
					  _world
					, _entity
					, _since
					)
				;
		}
	};

	template<typename... TermsT>
	struct EcsTermT<OrT<TermsT...> >
	{
		static const bool kFilter = true;
		static const bool kRequired = false;
		static uint32_t prepare(World& _world)
		{
			EcsOrT<TermsT...>::prepare(_world);
			return UINT32_MAX;
		}

		static bool matches(
			  World& _world
			, uint32_t
			, Entity _entity
			, uint64_t _since
			)
		{
			return EcsOrT<TermsT...>::matches(_world, _entity, _since);
		}
	};

	template<typename... TermsT>
	struct EcsInvokeT;

	template<>
	struct EcsInvokeT<>
	{
		template<typename FnT, typename... ArgsT>
		static void run(
			  World&
			, const uint32_t*
			, Entity
			, uint64_t
			, FnT& _fn
			, ArgsT&&... _args
			)
		{
			_fn(frost::forward<ArgsT>(_args)...);
		}
	};

	template<bool FilterT, typename TermT, typename... TermsT>
	struct EcsInvokeStepT;

	template<typename TermT, typename... TermsT>
	struct EcsInvokeStepT<true, TermT, TermsT...>
	{
		template<typename FnT, typename... ArgsT>
		static void run(
			  World& _world
			, const uint32_t* _pools
			, Entity _entity
			, uint64_t _since
			, FnT& _fn
			, ArgsT&&... _args
			)
		{
			EcsInvokeT<TermsT...>::run(
				  _world
				, _pools + 1
				, _entity
				, _since
				, _fn
				, frost::forward<ArgsT>(_args)...
				);
		}
	};

	template<typename TermT, typename... TermsT>
	struct EcsInvokeStepT<false, TermT, TermsT...>
	{
		template<typename FnT, typename... ArgsT>
		static void run(
			  World& _world
			, const uint32_t* _pools
			, Entity _entity
			, uint64_t _since
			, FnT& _fn
			, ArgsT&&... _args
			)
		{
			EcsInvokeT<TermsT...>::run(
				  _world
				, _pools + 1
				, _entity
				, _since
				, _fn
				, frost::forward<ArgsT>(_args)...
				, EcsTermT<TermT>::get(_world, *_pools, _entity, _since)
				);
		}
	};

	template<typename TermT, typename... TermsT>
	struct EcsInvokeT<TermT, TermsT...>
		: public EcsInvokeStepT<EcsTermT<TermT>::kFilter, TermT, TermsT...>
	{
	};

	template<typename... TermsT>
	struct EcsQueryTermsT;

	template<>
	struct EcsQueryTermsT<>
	{
		static void prepare(World&, uint32_t* _pools)
		{
			*_pools = UINT32_MAX;
		}

		static bool matches(
			  World&
			, const uint32_t*
			, Entity
			, uint64_t
			)
		{
			return true;
		}

		static void select(
			  World&
			, const uint32_t*
			, uint32_t&
			, uint32_t&
			)
		{
		}
	};

	template<typename TermT, typename... TermsT>
	struct EcsQueryTermsT<TermT, TermsT...>
	{
		static void prepare(World& _world, uint32_t* _pools)
		{
			*_pools = EcsTermT<TermT>::prepare(_world);
			EcsQueryTermsT<TermsT...>::prepare(_world, _pools + 1);
		}

		static bool matches(
			  World& _world
			, const uint32_t* _pools
			, Entity _entity
			, uint64_t _since
			)
		{
			return EcsTermT<TermT>::matches(_world, *_pools, _entity, _since)
				&& EcsQueryTermsT<TermsT...>::matches(
					  _world
					, _pools + 1
					, _entity
					, _since
					)
				;
		}

		static void select(
			  World& _world
			, const uint32_t* _pools
			, uint32_t& _pool
			, uint32_t& _count
			)
		{
			if (EcsTermT<TermT>::kRequired)
			{
				const uint32_t count = EcsAccess::count(_world, *_pools);
				if (count <= _count)
				{
					_count = count;
					_pool  = *_pools;
				}
			}

			EcsQueryTermsT<TermsT...>::select(
				  _world
				, _pools + 1
				, _pool
				, _count
				);
		}
	};

	template<typename Ty>
	struct EcsParamT
	{
		static const bool kExclusive = false;
		static bool ready(World&)
		{
			return true;
		}

		static Ty get(World& _world, void*&)
		{
			return Ty(_world);
		}
	};

	template<>
	struct EcsParamT<World&>
	{
		static const bool kExclusive = true;
		static bool ready(World&)
		{
			return true;
		}

		static World& get(World& _world, void*&)
		{
			return _world;
		}
	};

	template<>
	struct EcsParamT<const World&>
	{
		static const bool kExclusive = true;
		static bool ready(World&)
		{
			return true;
		}

		static const World& get(World& _world, void*&)
		{
			return _world;
		}
	};

	template<typename Ty>
	struct EcsParamT<LocalT<Ty> >
	{
		static const bool kExclusive = false;
		static bool ready(World&)
		{
			return true;
		}

		static LocalT<Ty> get(World& _world, void*& _locals)
		{
			return LocalT<Ty>((Ty*)EcsAccess::local(_world, _locals,
				getEcsType<Ty>(), &EcsTypeT<Ty>::init) );
		}
	};

	template<typename Ty>
	struct EcsParamT<MessageReaderT<Ty> >
	{
		static const bool kExclusive = false;
		static bool ready(World&)
		{
			return true;
		}

		static MessageReaderT<Ty> get(World& _world, void*& _locals)
		{
			LocalT<MessageCursorT<Ty> > cursor = EcsParamT<LocalT<MessageCursorT<Ty> > >::get(_world, _locals);
			return MessageReaderT<Ty>(_world, *cursor);
		}
	};

	template<typename... ArgsT>
	struct EcsParamsT;

	template<>
	struct EcsParamsT<>
	{
		static const bool kExclusive = false;

		static bool ready(World&)
		{
			return true;
		}
	};

	template<typename ArgT, typename... ArgsT>
	struct EcsParamsT<ArgT, ArgsT...>
	{
		static const bool kExclusive =
			EcsParamT<ArgT>::kExclusive || EcsParamsT<ArgsT...>::kExclusive;

		static bool ready(World& _world)
		{
			return EcsParamT<ArgT>::ready(_world) && EcsParamsT<ArgsT...>::ready(_world);
		}
	};

	template<typename... ArgsT>
	struct EcsSystemT
	{
		void (*m_fn)(ArgsT...);
		static bool ready(World& _world)
		{
			return EcsParamsT<ArgsT...>::ready(_world);
		}

		static void run(
			  World& _world
			, void* _ptr
			, void*& _locals
			)
		{
			((EcsSystemT*)_ptr)->m_fn(EcsParamT<ArgsT>::get(_world, _locals)...);
		}
	};

	template<typename Ty>
	struct EcsParamT<ResT<Ty> >
	{
		static const bool kExclusive = false;
		static bool ready(World& _world)
		{
			return NULL != _world.getResource<Ty>();
		}

		static ResT<Ty> get(World& _world, void*&)
		{
			return ResT<Ty>(_world);
		}
	};

	template<typename Ty>
	struct EcsParamT<ResMutT<Ty> > : EcsParamT<ResT<Ty> >
	{
		static ResMutT<Ty> get(World& _world, void*&)
		{
			return ResMutT<Ty>(_world);
		}
	};

	template<typename Ty>
	struct EcsStateT
	{
		EcsStateT()
			: m_initialized(false)
		{
		}

		bool m_initialized;
		static bool equal(const void* _lhs, const void* _rhs)
		{
			return *(const Ty*)_lhs == *(const Ty*)_rhs;
		}

		static bool run(App& _app, void* _ptr)
		{
			World& world = _app.getWorld();
			EcsStateT* self = (EcsStateT*)_ptr;
			Ty previous = world.getResource<StateT<Ty> >()->value;
			if (!self->m_initialized)
			{
				self->m_initialized = true;
				if (!EcsAccess::runState(_app, getEcsType<Ty>(), &previous, true) )
				{
					return false;
				}
			}
			const NextStateT<Ty>* next = world.getResource<NextStateT<Ty> >();
			if (!next->pending)
			{
				return true;
			}
			const Ty value = next->value;
			world.getResourceMut<NextStateT<Ty> >()->pending = false;
			if (previous == value)
			{
				return true;
			}

			if (!EcsAccess::runState(_app, getEcsType<Ty>(), &previous, false) )
			{
				return false;
			}

			world.getResourceMut<StateT<Ty> >()->value = value;
			return EcsAccess::runState(_app, getEcsType<Ty>(), &value, true);
		}
	};

	inline void ecsInsertBundle(World&, Entity)
	{
	}

	template<typename Ty, typename... ArgsT>
	inline void ecsInsertBundle(
		  World& _world
		, Entity _entity
		, Ty&& _value
		, ArgsT&&... _args
		)
	{
		_world.insert(_entity, frost::forward<Ty>(_value) );
		ecsInsertBundle(_world, _entity, frost::forward<ArgsT>(_args)...);
	}

	inline void ecsInsertBundle(EntityCommands&)
	{
	}

	template<typename Ty, typename... ArgsT>
	inline void ecsInsertBundle(
		  EntityCommands& _entity
		, Ty&& _value
		, ArgsT&&... _args
		)
	{
		_entity.insert(frost::forward<Ty>(_value) );
		ecsInsertBundle(_entity, frost::forward<ArgsT>(_args)...);
	}

	template<typename Ty>
	struct EcsRemovedT
	{
		static void write(World& _world, Entity _entity)
		{
			_world.writeMessage(RemovedComponentT<Ty>(_entity) );
		}
	};

	template<typename Ty>
	struct EcsRequiredT
	{
		static void insert(World& _world, Entity _entity)
		{
			_world.emplace<Ty>(_entity);
		}
	};

	template<typename FnT>
	struct EcsCommandT
	{
		static void run(World& _world, void* _ptr)
		{
			(*(FnT*)_ptr)(_world);
		}
	};

	struct EcsActivateCommand
	{
		explicit EcsActivateCommand(Entity _entity)
			: m_entity(_entity)
		{
		}

		void operator()(World& _world)
		{
			EcsAccess::activate(_world, m_entity);
		}

		Entity m_entity;
	};

	struct EcsFunctionCommand
	{
		explicit EcsFunctionCommand(EcsCommandFn _fn )
			: m_fn(_fn)
		{
		}

		void operator()(World& _world)
		{
			m_fn(_world);
		}

		EcsCommandFn m_fn;
	};

	template<typename Ty>
	struct EcsInsertCommandT
	{
		EcsInsertCommandT(Entity _entity, Ty&& _value)
			: m_entity(_entity)
			, m_value(frost::move(_value) )
		{
		}

		void operator()(World& _world)
		{
			_world.insert(m_entity, frost::move(m_value) );
		}

		Entity m_entity;
		Ty     m_value;
	};

	template<typename Ty>
	struct EcsResourceCommandT
	{
		explicit EcsResourceCommandT(Ty&& _value)
			: m_value(frost::move(_value) )
		{
		}

		void operator()(World& _world)
		{
			_world.insertResource<Ty>(frost::move(m_value) );
		}

		Ty m_value;
	};

	template<typename Ty>
	struct EcsTriggerCommandT
	{
		EcsTriggerCommandT(Ty&& _value, Entity _target)
			: m_value(frost::move(_value) )
			, m_target(_target)
		{
		}

		void operator()(World& _world)
		{
			_world.trigger(m_value, m_target);
		}

		Ty     m_value;
		Entity m_target;
	};

	template<typename Ty>
	struct EcsRemoveCommandT
	{
		explicit EcsRemoveCommandT(Entity _entity)
			: m_entity(_entity)
		{
		}

		void operator()(World& _world)
		{
			_world.remove<Ty>(m_entity);
		}

		Entity m_entity;
	};

	struct EcsDespawnCommand
	{
		EcsDespawnCommand(Entity _entity, bool _recursive)
			: m_entity(_entity)
			, m_recursive(_recursive)
		{
		}

		void operator()(World& _world)
		{
			_world.despawn(m_entity, m_recursive);
		}

		Entity m_entity;
		bool   m_recursive;
	};

	struct EcsParentCommand
	{
		EcsParentCommand(Entity _entity, Entity _parent)
			: m_entity(_entity)
			, m_parent(_parent)
		{
		}

		void operator()(World& _world)
		{
			_world.setParent(m_entity, m_parent);
		}

		Entity m_entity;
		Entity m_parent;
	};

	template<typename Ty, typename FnT>
	struct EcsReadT
	{
		static void run(const void* _ptr, void* _userData)
		{
			(*(FnT*)_userData)(*(const Ty*)_ptr);
		}
	};

	template<typename Ty>
	struct EcsObserverT
	{
		typedef void (*ObserverFn)(World&, Entity, const Ty&, void*);

		EcsObserverT(ObserverFn _fn, void* _userData)
			: m_fn(_fn)
			, m_userData(_userData)
		{
		}

		static void run(
			  World& _world
			, Entity _entity
			, const void* _event
			, void* _ptr
			)
		{
			EcsObserverT* self = (EcsObserverT*)_ptr;
			self->m_fn(_world, _entity, *(const Ty*)_event, self->m_userData);
		}

		ObserverFn m_fn;
		void*      m_userData;
	};

	template<typename Ty>
	inline const EcsType* getEcsType()
	{
		typedef typename EcsValueT<Ty>::Type ValueType;
		return EcsTypeIdT<ValueType>::get();
	}

	inline Entity::Entity()
		: idx(UINT32_MAX)
		, generation(0)
	{
	}

	inline Entity::Entity(uint32_t _idx, uint32_t _generation)
		: idx(_idx)
		, generation(_generation)
	{
	}

	inline SystemId::SystemId()
		: idx(UINT32_MAX)
	{
	}

	inline SystemId::SystemId(uint32_t _idx)
		: idx(_idx)
	{
	}

	inline Time::Time()
		: delta(0.0)
		, elapsed(0.0)
		, fixedDelta(1.0/64.0)
		, fixedElapsed(0.0)
		, accumulator(0.0)
		, frame(0)
	{
	}

	template<typename Ty>
	inline MessageCursorT<Ty>::MessageCursorT()
		: sequence(0)
	{
	}

	template<typename Ty>
	inline RemovedComponentT<Ty>::RemovedComponentT()
	{
	}

	template<typename Ty>
	inline RemovedComponentT<Ty>::RemovedComponentT(Entity _entity)
		: entity(_entity)
	{
	}

	template<typename Ty>
	inline StateT<Ty>::StateT()
		: value()
	{
	}

	template<typename Ty>
	inline StateT<Ty>::StateT(Ty _value)
		: value(_value)
	{
	}

	template<typename Ty>
	inline NextStateT<Ty>::NextStateT()
		: value()
		, pending(false)
	{
	}

	template<typename Ty>
	inline NextStateT<Ty>::NextStateT(Ty _value, bool _pending)
		: value(_value)
		, pending(_pending)
	{
	}

	inline bool Entity::operator==(Entity _other) const
	{
		return idx == _other.idx && generation == _other.generation;
	}

	inline bool Entity::operator!=(Entity _other) const
	{
		return !(*this == _other);
	}

	template<typename... TermsT>
	inline QueryT<TermsT...>::QueryT(World& _world)
		: m_world(&_world)
		, m_since(EcsAccess::since(_world) )
	{
		EcsQueryTermsT<TermsT...>::prepare(_world, m_pools);
	}

	template<typename... TermsT>
	inline bool QueryT<TermsT...>::contains(Entity _entity) const
	{
		return m_world->contains(_entity)
			&& EcsQueryTermsT<TermsT...>::matches(
				  *m_world
				, m_pools
				, _entity
				, m_since
				)
			;
	}

	template<typename... TermsT>
	template<typename FnT>
	inline void QueryT<TermsT...>::each(FnT&& _fn) const
	{
		EcsIteration iteration(*m_world);
		uint32_t pool = UINT32_MAX;
		uint32_t count = EcsAccess::extent(*m_world);
		EcsQueryTermsT<TermsT...>::select(
			  *m_world
			, m_pools
			, pool
			, count
			);

		for (uint32_t ii = 0; ii < count; ++ii)
		{
			const Entity entity = UINT32_MAX == pool
				? EcsAccess::at(*m_world, ii)
				: EcsAccess::at(
					  *m_world
					, pool
					, ii
					);
			if (contains(entity) )
			{
				EcsInvokeT<TermsT...>::run(
					  *m_world
					, m_pools
					, entity
					, m_since
					, _fn
					);
			}
		}
	}

	template<typename... TermsT>
	template<typename FnT>
	inline bool QueryT<TermsT...>::get(Entity _entity, FnT&& _fn) const
	{
		if (!contains(_entity) )
		{
			return false;
		}

		EcsIteration iteration(*m_world);
		EcsInvokeT<TermsT...>::run(
			  *m_world
			, m_pools
			, _entity
			, m_since
			, _fn
			);
		return true;
	}

	template<typename... TermsT>
	inline uint32_t QueryT<TermsT...>::getCount() const
	{
		uint32_t count = 0;
		const uint32_t extent = EcsAccess::extent(*m_world);
		for (uint32_t ii = 0; ii < extent; ++ii)
		{
			count += contains(EcsAccess::at(*m_world, ii) ) ? 1 : 0;
		}
		return count;
	}

	template<typename... TermsT>
	template<typename FnT>
	inline bool QueryT<TermsT...>::single(FnT&& _fn) const
	{
		Entity result;
		const uint32_t extent = EcsAccess::extent(*m_world);
		for (uint32_t ii = 0; ii < extent; ++ii)
		{
			const Entity entity = EcsAccess::at(*m_world, ii);
			if (contains(entity) )
			{
				if (UINT32_MAX != result.idx)
				{
					return false;
				}
				result = entity;
			}
		}

		return get(result, frost::forward<FnT>(_fn) );
	}

	template<typename... TermsT>
	template<typename FnT>
	inline bool QueryT<TermsT...>::getMany(
		  const Entity* _entities
		, uint32_t _count
		, FnT&& _fn
		) const
	{
		for (uint32_t ii = 0; ii < _count; ++ii)
		{
			if (!contains(_entities[ii]) )
			{
				return false;
			}

			for (uint32_t jj = 0; jj < ii; ++jj)
			{
				if (_entities[ii] == _entities[jj])
				{
					return false;
				}
			}
		}

		EcsIteration iteration(*m_world);
		for (uint32_t ii = 0; ii < _count; ++ii)
		{
			EcsInvokeT<TermsT...>::run(
				  *m_world
				, m_pools
				, _entities[ii]
				, m_since
				, _fn
				);
		}
		return true;
	}

	template<typename Ty>
	inline RefT<Ty>::RefT(
		  World& _world
		, uint32_t _pool
		, Entity _entity
		, uint64_t _since
		)
		: m_world(&_world)
		, m_pool(_pool)
		, m_entity(_entity)
		, m_since(_since)
	{
	}

	template<typename Ty>
	inline const Ty* RefT<Ty>::operator->() const
	{
		return (const Ty*)EcsAccess::component(*m_world, m_pool, m_entity);
	}

	template<typename Ty>
	inline const Ty& RefT<Ty>::operator*() const
	{
		return *operator->();
	}

	template<typename Ty>
	inline bool RefT<Ty>::isAdded() const
	{
		return EcsAccess::changed(*m_world, m_pool, m_entity, m_since, true);
	}

	template<typename Ty>
	inline bool RefT<Ty>::isChanged() const
	{
		return EcsAccess::changed(*m_world, m_pool, m_entity, m_since, false);
	}

	template<typename Ty>
	inline MutT<Ty>::MutT(
		  World& _world
		, uint32_t _pool
		, Entity _entity
		, uint64_t _since
		)
		: RefT<Ty>(
			  _world
			, _pool
			, _entity
			, _since
			)
	{
	}

	template<typename Ty>
	inline Ty* MutT<Ty>::operator->()
	{
		return (Ty*)EcsAccess::component(*this->m_world, this->m_pool, this->m_entity, true);
	}

	template<typename Ty>
	inline Ty& MutT<Ty>::operator*()
	{
		return *operator->();
	}

	template<typename Ty>
	inline const Ty& MutT<Ty>::get() const
	{
		return RefT<Ty>::operator*();
	}

	template<typename... ComponentsT>
	inline EntityMut World::spawn(ComponentsT&&... _components)
	{
		const Entity id = EcsAccess::create(*this);
		if (contains(id) )
		{
			ecsInsertBundle(*this, id, frost::forward<ComponentsT>(_components)...);
		}

		return EntityMut(*this, id);
	}

	template<typename Ty, typename... ArgsT>
	inline Ty* World::emplace(Entity _entity, ArgsT&&... _args)
	{
		Ty value(frost::forward<ArgsT>(_args)...);
		static_assert(isConstructible<Ty, Ty&&>(), "Components must be move constructible.");
		EcsAccess::trackRemoval(*this, getEcsType<Ty>(), &EcsRemovedT<Ty>::write);
		return (Ty*)EcsAccess::insert(*this, getEcsType<Ty>(), _entity, &value);
	}

	template<typename Ty>
	inline RemoveCvType<RemoveReferenceType<Ty> >* World::insert(Entity _entity, Ty&& _value)
	{
		typedef typename EcsValueT<Ty>::Type ValueType;
		ValueType value(frost::forward<Ty>(_value) );
		static_assert(isConstructible<ValueType, ValueType&&>(), "Components must be move constructible.");
		EcsAccess::trackRemoval(*this, getEcsType<ValueType>(), &EcsRemovedT<ValueType>::write);
		return (ValueType*)EcsAccess::insert(*this, getEcsType<ValueType>(), _entity, &value);
	}

	template<typename Ty>
	inline const Ty* World::get(Entity _entity) const
	{
		World& world = *const_cast<World*>(this);
		return (const Ty*)EcsAccess::component(world,
			EcsAccess::pool(world, getEcsType<Ty>() ), _entity);
	}

	template<typename Ty>
	inline Ty* World::getMut(Entity _entity)
	{
		return (Ty*)EcsAccess::component(*this, EcsAccess::pool(*this, getEcsType<Ty>() ), _entity, true);
	}

	template<typename Ty>
	inline bool World::has(Entity _entity) const
	{
		return NULL != get<Ty>(_entity);
	}

	template<typename Ty>
	inline bool World::remove(Entity _entity)
	{
		return EcsAccess::remove(*this, getEcsType<Ty>(), _entity);
	}

	template<typename Ty, typename RequiredT>
	inline bool World::registerRequired()
	{
		return EcsAccess::required(*this, getEcsType<Ty>(), getEcsType<RequiredT>(),
			&EcsRequiredT<RequiredT>::insert);
	}

	template<typename Ty>
	inline void World::setComponentHook(
		  EcsComponentHookFn _fn
		, void* _userData
		)
	{
		EcsAccess::hook(*this, getEcsType<Ty>(), _fn, _userData);
	}

	template<typename... TermsT>
	inline QueryT<TermsT...> World::query()
	{
		return QueryT<TermsT...>(*this);
	}

	template<typename Ty, typename... ArgsT>
	inline Ty& World::insertResource(ArgsT&&... _args)
	{
		Ty value(frost::forward<ArgsT>(_args)...);
		return *(Ty*)EcsAccess::insertResource(*this, getEcsType<Ty>(), &value);
	}

	template<typename Ty>
	inline Ty& World::initResource()
	{
		Ty* ptr = (Ty*)EcsAccess::resource(*this, getEcsType<Ty>(), true);
		return NULL == ptr ? insertResource<Ty>() : *ptr;
	}

	template<typename Ty>
	inline const Ty* World::getResource() const
	{
		return (const Ty*)EcsAccess::resource(*const_cast<World*>(this), getEcsType<Ty>() );
	}

	template<typename Ty>
	inline Ty* World::getResourceMut()
	{
		return (Ty*)EcsAccess::resource(*this, getEcsType<Ty>(), true);
	}

	template<typename Ty>
	inline bool World::removeResource()
	{
		return EcsAccess::removeResource(*this, getEcsType<Ty>() );
	}

	template<typename Ty>
	inline ResT<Ty>::ResT(World& _world)
		: m_world(&_world)
		, m_ptr(_world.getResource<Ty>() )
		, m_since(EcsAccess::since(_world) )
	{
		FROST_ASSERT(NULL != m_ptr, "Required ECS resource is missing.");
	}

	template<typename Ty>
	inline const Ty* ResT<Ty>::operator->() const
	{
		return m_ptr;
	}

	template<typename Ty>
	inline const Ty& ResT<Ty>::operator*() const
	{
		return *m_ptr;
	}

	template<typename Ty>
	inline bool ResT<Ty>::isAdded() const
	{
		return EcsAccess::resourceChanged(*m_world, getEcsType<Ty>(), m_since, true);
	}

	template<typename Ty>
	inline bool ResT<Ty>::isChanged() const
	{
		return EcsAccess::resourceChanged(*m_world, getEcsType<Ty>(), m_since, false);
	}

	template<typename Ty>
	inline ResMutT<Ty>::ResMutT(World& _world)
		: ResT<Ty>(_world)
	{
	}

	template<typename Ty>
	inline Ty* ResMutT<Ty>::operator->()
	{
		return this->m_world->template getResourceMut<Ty>();
	}

	template<typename Ty>
	inline Ty& ResMutT<Ty>::operator*()
	{
		return *operator->();
	}

	template<typename Ty>
	inline const Ty& ResMutT<Ty>::get() const
	{
		return *this->m_ptr;
	}

	template<typename Ty>
	inline LocalT<Ty>::LocalT(Ty* _ptr)
		: m_ptr(_ptr)
	{
	}

	template<typename Ty>
	inline Ty* LocalT<Ty>::operator->()
	{
		return m_ptr;
	}

	template<typename Ty>
	inline Ty& LocalT<Ty>::operator*()
	{
		return *m_ptr;
	}

	template<typename Ty>
	inline void NextStateT<Ty>::set(Ty _value)
	{
		value = _value;
		pending = true;
	}

	inline EntityMut::EntityMut(World& _world, Entity _entity)
		: m_world(&_world)
		, m_entity(_entity)
	{
	}

	inline Entity EntityMut::id() const
	{
		return m_entity;
	}

	inline EntityMut::operator Entity() const
	{
		return m_entity;
	}

	template<typename Ty>
	inline EntityMut& EntityMut::insert(Ty&& _value)
	{
		m_world->insert(m_entity, frost::forward<Ty>(_value) );
		return *this;
	}

	template<typename Ty>
	inline EntityMut& EntityMut::remove()
	{
		m_world->remove<Ty>(m_entity);
		return *this;
	}

	template<typename Ty>
	inline const Ty* EntityMut::get() const
	{
		return m_world->get<Ty>(m_entity);
	}

	template<typename Ty>
	inline Ty* EntityMut::getMut()
	{
		return m_world->getMut<Ty>(m_entity);
	}

	inline bool EntityMut::despawn(bool _recursive)
	{
		return m_world->despawn(m_entity, _recursive);
	}

	inline EntityMut& EntityMut::setParent(Entity _parent)
	{
		m_world->setParent(m_entity, _parent);
		return *this;
	}

	inline EntityMut World::entity(Entity _entity)
	{
		return EntityMut(*this, _entity);
	}

	inline Commands World::commands()
	{
		return Commands(*this);
	}

	inline Commands::Commands(World& _world)
		: m_world(&_world)
	{
	}

	inline EntityCommands Commands::entity(Entity _entity)
	{
		return EntityCommands(*m_world, _entity);
	}

	template<typename FnT>
	inline void Commands::queue(FnT&& _fn)
	{
		typedef typename EcsValueT<FnT>::Type ValueType;
		ValueType fn(frost::forward<FnT>(_fn) );
		EcsAccess::queue(*m_world, getEcsType<ValueType>(), &fn, &EcsCommandT<ValueType>::run);
	}

	template<typename... ComponentsT>
	inline EntityCommands Commands::spawn(ComponentsT&&... _components)
	{
		const Entity id = EcsAccess::reserve(*m_world);
		queue(EcsActivateCommand(id) );
		EntityCommands result(*m_world, id);
		ecsInsertBundle(result, frost::forward<ComponentsT>(_components)...);
		return result;
	}

	template<typename Ty>
	inline Commands& Commands::insertResource(Ty&& _value)
	{
		typedef typename EcsValueT<Ty>::Type ValueType;
		queue(EcsResourceCommandT<ValueType>(ValueType(frost::forward<Ty>(_value) ) ) );
		return *this;
	}

	template<typename Ty>
	inline Commands& Commands::trigger(Ty _event, Entity _target)
	{
		queue(EcsTriggerCommandT<Ty>(frost::move(_event), _target) );
		return *this;
	}

	inline EntityCommands::EntityCommands(World& _world, Entity _entity)
		: m_world(&_world)
		, m_entity(_entity)
	{
	}

	inline Entity EntityCommands::id() const
	{
		return m_entity;
	}

	template<typename Ty>
	inline EntityCommands& EntityCommands::insert(Ty&& _value)
	{
		typedef typename EcsValueT<Ty>::Type ValueType;
		Commands(*m_world).queue(
			  EcsInsertCommandT<ValueType>(m_entity, ValueType(frost::forward<Ty>(_value) ) )
			);
		return *this;
	}

	template<typename Ty>
	inline EntityCommands& EntityCommands::remove()
	{
		Commands(*m_world).queue(EcsRemoveCommandT<Ty>(m_entity) );
		return *this;
	}

	inline EntityCommands& EntityCommands::despawn(bool _recursive)
	{
		Commands(*m_world).queue(EcsDespawnCommand(m_entity, _recursive) );
		return *this;
	}

	inline EntityCommands& EntityCommands::setParent(Entity _parent)
	{
		Commands(*m_world).queue(EcsParentCommand(m_entity, _parent) );
		return *this;
	}

	template<typename Ty>
	inline void World::writeMessage(Ty _value)
	{
		EcsAccess::message(*this, getEcsType<Ty>(), &_value);
	}

	template<typename Ty>
	inline MessageReaderT<Ty>::MessageReaderT(World& _world, MessageCursorT<Ty>& _cursor)
		: m_world(&_world)
		, m_cursor(&_cursor)
	{
	}

	template<typename Ty>
	template<typename FnT>
	inline void MessageReaderT<Ty>::each(FnT&& _fn)
	{
		EcsAccess::read(*m_world, getEcsType<Ty>(), m_cursor->sequence,
			&EcsReadT<Ty, RemoveReferenceType<FnT> >::run, (void*)&_fn);
	}

	template<typename Ty>
	inline MessageWriterT<Ty>::MessageWriterT(World& _world)
		: m_world(&_world)
	{
	}

	template<typename Ty>
	inline void MessageWriterT<Ty>::write(Ty _value)
	{
		m_world->writeMessage(frost::move(_value) );
	}

	template<typename Ty>
	inline uint32_t World::addObserver(
		  void (*_fn)(World&, Entity, const Ty&, void*)
		, void* _userData
		, Entity _target
		)
	{
		EcsObserverT<Ty> value(_fn, _userData);
		return EcsAccess::observe(*this, getEcsType<Ty>(), getEcsType<EcsObserverT<Ty> >(),
			&value, &EcsObserverT<Ty>::run, _target);
	}

	template<typename Ty>
	inline void World::trigger(const Ty& _event, Entity _target)
	{
		EcsAccess::trigger(*this, getEcsType<Ty>(), &_event, _target);
	}

	template<typename... ArgsT>
	inline SystemConfig Schedule::addSystems(void (*_fn)(ArgsT...))
	{
		EcsSystemT<ArgsT...> value = { _fn };
		return EcsAccess::system(*this, getEcsType<EcsSystemT<ArgsT...> >(), &value,
			&EcsSystemT<ArgsT...>::run, &EcsSystemT<ArgsT...>::ready,
			EcsParamsT<ArgsT...>::kExclusive);
	}

	template<typename LabelT>
	inline Schedule& App::getSchedule()
	{
		return EcsAccess::schedule(*this, getEcsType<LabelT>() );
	}

	template<typename... ArgsT>
	inline SystemConfig App::addSystems(EcsStage::Enum _stage, void (*_fn)(ArgsT...))
	{
		return getSchedule(_stage).addSystems(_fn);
	}

	template<typename Ty, typename... ArgsT>
	inline App& App::insertResource(ArgsT&&... _args)
	{
		getWorld().insertResource<Ty>(frost::forward<ArgsT>(_args)...);
		return *this;
	}

	template<typename Ty>
	inline App& App::initState(Ty _initial)
	{
		if (NULL == getWorld().getResource<StateT<Ty> >() )
		{
			getWorld().insertResource<StateT<Ty> >(StateT<Ty>(_initial));
			getWorld().insertResource<NextStateT<Ty> >(NextStateT<Ty>(_initial, false));
			EcsStateT<Ty> state;
			EcsAccess::state(*this, getEcsType<EcsStateT<Ty> >(), &state, &EcsStateT<Ty>::run);
		}
		return *this;
	}

	template<typename Ty, Ty ValueT>
	inline bool App::inState(const World& _world, void* _userData)
	{
		FROST_UNUSED(_userData);
		const StateT<Ty>* state = _world.getResource<StateT<Ty> >();
		return NULL != state && state->value == ValueT;
	}

	template<typename Ty, typename... ArgsT>
	inline SystemConfig App::onEnter(Ty _state, void (*_fn)(ArgsT...))
	{
		return EcsAccess::stateSchedule(*this, getEcsType<Ty>(), &_state, true,
			&EcsStateT<Ty>::equal).addSystems(_fn);
	}

	template<typename Ty, typename... ArgsT>
	inline SystemConfig App::onExit(Ty _state, void (*_fn)(ArgsT...))
	{
		return EcsAccess::stateSchedule(*this, getEcsType<Ty>(), &_state, false,
			&EcsStateT<Ty>::equal).addSystems(_fn);
	}

	template<typename PluginT, typename... ArgsT>
	inline bool App::addPlugins(ArgsT&&... _args)
	{
		AllocatorI* allocator = getWorld().getAllocator();
		void* ptr = frost::alloc(allocator, sizeof(PluginT), FROST_ALIGNOF(PluginT) );
		PluginT* plugin = FROST_PLACEMENT_NEW(ptr, PluginT)(frost::forward<ArgsT>(_args)...);
		return EcsAccess::plugin(*this, plugin, getEcsType<PluginT>(), ptr);
	}

	template<typename PluginT>
	inline bool App::isPluginAdded() const
	{
		return EcsAccess::hasPlugin(*this, getEcsType<PluginT>() );
	}

} // namespace frost
