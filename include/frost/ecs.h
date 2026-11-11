/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_ECS_H_HEADER_GUARD
#define FROST_ECS_H_HEADER_GUARD

#include "allocator.h"

namespace frost
{
	class World;
	class App;
	class Commands;
	class EntityMut;
	class EntityCommands;
	class Schedule;
	class SystemConfig;
	struct EcsAccess;

	/// Entity index and generation. Handles belong to the World that created them.
	///
	struct Entity
	{
		///
		Entity();

		///
		Entity(uint32_t _idx, uint32_t _generation);

		///
		bool operator==(Entity _other) const;

		///
		bool operator!=(Entity _other) const;

		uint32_t idx;        //!< Entity index.
		uint32_t generation; //!< Entity generation.
	};

	/// Startup and frame schedules, followed by Shutdown when the application stops.
	/// Fixed schedules use TimeResource::fixedDelta.
	///
	struct EcsStage
	{
		///
		enum Enum
		{
			PreStartup,
			Startup,
			PostStartup,
			First,
			PreUpdate,
			FixedFirst,
			FixedPreUpdate,
			FixedUpdate,
			FixedPostUpdate,
			FixedLast,
			Update,
			PostUpdate,
			Last,
			Shutdown, //!< Runs once before plugins and resources are destroyed.

			Count
		};
	};

	/// Component lifecycle hooks run before destruction and after construction, respectively.
	///
	struct EcsLifecycle
	{
		///
		enum Enum
		{
			Add,
			Insert,
			Replace,
			Remove,
			Despawn,

			Count
		};
	};

	/// Schedule execution result.
	///
	struct EcsScheduleResult
	{
		///
		enum Enum
		{
			Success,
			Cycle,
			Busy,
			WorldMismatch,
			MissingResource,

			Count
		};
	};

	///
	typedef void (*EcsMoveFn)(void* _dst, void* _src);

	///
	typedef void (*EcsDestroyFn)(void* _ptr);

	///
	typedef void (*EcsComponentHookFn)(World&, Entity, EcsLifecycle::Enum, void*);

	///
	typedef bool (*EcsRunConditionFn)(const World&, void*);

	///
	typedef void (*EcsCommandFn)(World&);

	///
	typedef void (*EcsRunnerFn)(App&, void*);

	/// Type operations use FROST placement new and the owning World's allocator, without RTTI.
	///
	struct EcsType
	{
		uint32_t     size;
		uint32_t     align;
		EcsMoveFn    move;
		EcsDestroyFn destroy;
	};

	/// Type identity is process local. Do not serialize it or pass it across shared libraries.
	///
	template<typename Ty>
	const EcsType* getEcsType();

	/// Component presence filters do not add arguments to QueryT::each callbacks.
	///
	template<typename Ty>
	struct WithT
	{
	};
	/// Exclude entities containing Ty.
	///
	template<typename Ty>
	struct WithoutT
	{
	};
	/// Match components added since the system last ran.
	///
	template<typename Ty>
	struct AddedT
	{
	};
	/// Match components changed since the system last ran.
	///
	template<typename Ty>
	struct ChangedT
	{
	};
	/// Match any of the specified filters.
	///
	template<typename... FiltersT>
	struct OrT
	{
	};

	/// Optional component query data. The callback receives Ty*, or NULL when absent.
	///
	template<typename Ty>
	struct OptionalT
	{
	};

	/// Component access with change information. MutT marks changed on mutable dereference.
	///
	template<typename Ty>
	class RefT
	{
	public:
		///
		RefT(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			);

		///
		const Ty* operator->() const;

		///
		const Ty& operator*() const;

		///
		bool isAdded() const;

		///
		bool isChanged() const;

	protected:
		World*   m_world;
		uint32_t m_pool;
		Entity   m_entity;
		uint64_t m_since;
	};

	/// Mutable component access with change detection.
	///
	template<typename Ty>
	class MutT : public RefT<Ty>
	{
	public:
		///
		MutT(
			  World& _world
			, uint32_t _pool
			, Entity _entity
			, uint64_t _since
			);

		///
		Ty* operator->();

		///
		Ty& operator*();

		/// Read without marking changed.
		///
		const Ty& get() const;
	};

	/// An entity from which component Ty was removed. Read through MessageReaderT.
	///
	template<typename Ty>
	struct RemovedComponentT
	{
		///
		RemovedComponentT();

		///
		explicit RemovedComponentT(Entity _entity);

		Entity entity;
	};

	/// Typed component query. Use references for data, Entity for IDs, and filter terms.
	///
	/// @attention Structural changes during each() must use Commands. Component pointers are
	///   invalidated by structural changes to their storage. Mutable references mark changed
	///   when fetched; const references do not. Queries and Worlds are single-threaded.
	///
	template<typename... TermsT>
	class QueryT
	{
	public:
		/// Uses the current system's change window, or the World's last clearTrackers().
		///
		QueryT(World& _world);

		/// Invoke callback with each matching entity's data, in template argument order.
		///
		template<typename FnT>
		void each(FnT&& _fn) const;

		/// Invoke callback for one matching entity. Returns false for stale or filtered IDs.
		///
		template<typename FnT>
		bool get(Entity _entity, FnT&& _fn) const;

		/// Invoke callback only when exactly one entity matches.
		///
		template<typename FnT>
		bool single(FnT&& _fn) const;

		/// Validate all IDs before invoking callbacks. Duplicate IDs are rejected.
		///
		template<typename FnT>
		bool getMany(
			  const Entity* _entities
			, uint32_t _count
			, FnT&& _fn
			) const;

		///
		bool contains(Entity _entity) const;

		///
		uint32_t getCount() const;

	private:
		World*   m_world;
		uint64_t m_since;
		uint32_t m_pools[sizeof...(TermsT) + 1];
	};

	/// Read-only resource system parameter. A missing resource is a programming error.
	///
	template<typename Ty>
	class ResT
	{
	public:
		///
		ResT(World& _world);

		///
		const Ty* operator->() const;

		///
		const Ty& operator*() const;

		///
		bool isAdded() const;

		///
		bool isChanged() const;

	protected:
		World*    m_world;
		const Ty* m_ptr;
		uint64_t  m_since;
	};

	/// Resource access marks changed only when mutably dereferenced.
	///
	/// Mutable resource access with change detection.
	///
	template<typename Ty>
	class ResMutT : public ResT<Ty>
	{
	public:
		///
		ResMutT(World& _world);

		///
		Ty* operator->();

		///
		Ty& operator*();

		/// Read without marking changed.
		///
		const Ty& get() const;
	};

	/// Persistent, default-constructed storage private to one system instance.
	///
	template<typename Ty>
	class LocalT
	{
	public:
		///
		explicit LocalT(Ty* _ptr);

		///
		Ty* operator->();

		///
		Ty& operator*();

	private:
		Ty* m_ptr;
	};

	/// Each reader owns an independent cursor. Unread messages expire after two updates.
	///
	template<typename Ty>
	struct MessageCursorT
	{
		///
		MessageCursorT();

		uint64_t sequence;
	};

	/// Read buffered messages with an independent cursor.
	///
	template<typename Ty>
	class MessageReaderT
	{
	public:
		/// Standalone readers must retain the cursor between calls.
		///
		MessageReaderT(World& _world, MessageCursorT<Ty>& _cursor);

		///
		template<typename FnT>
		void each(FnT&& _fn);

	private:
		World*              m_world;
		MessageCursorT<Ty>* m_cursor;
	};

	/// Write buffered messages.
	///
	template<typename Ty>
	class MessageWriterT
	{
	public:
		///
		MessageWriterT(World& _world);

		///
		void write(Ty _value);

	private:
		World* m_world;
	};

	/// Current state and a deferred transition request, stored as resources by App::initState.
	///
	template<typename Ty>
	struct StateT
	{
		///
		StateT();

		///
		explicit StateT(Ty _value);

		Ty value;
	};

	template<typename Ty>
	struct NextStateT
	{
		///
		NextStateT();

		///
		NextStateT(Ty _value, bool _pending = false);

		Ty   value;
		bool pending;

		/// Last request before transition processing wins.
		///
		void set(Ty _value);
	};

	/// Variable and fixed time, in seconds. App owns and updates this resource.
	///
	struct TimeResource
	{
		///
		TimeResource();

		double   delta;
		double   elapsed;
		double   fixedDelta;
		double   fixedElapsed;
		double   accumulator;
		uint64_t frame;
	};

	/// Allocator-owned ECS storage. Components and resources can have nontrivial lifetimes.
	///
	class World
	{
		FROST_CLASS(World
			, NO_COPY
			);

	public:
		/// Allocator must outlive the World and all of its owned data.
		///
		explicit World(AllocatorI* _allocator);

		/// Drops queued commands, messages, resources, and components.
		///
		~World();

		///
		AllocatorI* getAllocator() const;

		/// Create an entity and insert a bundle of components.
		///
		template<typename... ComponentsT>
		EntityMut spawn(ComponentsT&&... _components);

		///
		EntityMut entity(Entity _entity);

		/// Returns false for reserved, despawned, or stale entity IDs.
		///
		bool contains(Entity _entity) const;

		/// Despawn entity and, by default, its descendants.
		///
		bool despawn(Entity _entity, bool _recursive = true);

		///
		uint32_t getEntityCount() const;

		///
		template<typename Ty, typename... ArgsT>
		Ty* emplace(Entity _entity, ArgsT&&... _args);

		///
		template<typename Ty>
		RemoveCvType<RemoveReferenceType<Ty> >* insert(Entity _entity, Ty&& _value);

		///
		template<typename Ty>
		const Ty* get(Entity _entity) const;

		/// Marks component changed when present.
		///
		template<typename Ty>
		Ty* getMut(Entity _entity);

		///
		template<typename Ty>
		bool has(Entity _entity) const;

		///
		template<typename Ty>
		bool remove(Entity _entity);

		/// Register default-constructed required components. Rejects dependency cycles.
		///
		template<typename Ty, typename RequiredT>
		bool registerRequired();

		/// Hooks must enqueue structural changes through Commands.
		///
		template<typename Ty>
		void setComponentHook(EcsComponentHookFn _fn, void* _userData = NULL);

		///
		template<typename... TermsT>
		QueryT<TermsT...> query();

		///
		template<typename Ty, typename... ArgsT>
		Ty& insertResource(ArgsT&&... _args);

		/// Preserve an existing resource, or default-construct it.
		///
		template<typename Ty>
		Ty& initResource();

		///
		template<typename Ty>
		const Ty* getResource() const;

		///
		template<typename Ty>
		Ty* getResourceMut();

		///
		template<typename Ty>
		bool removeResource();

		/// Return a lightweight handle to this World's deferred command queue.
		///
		Commands commands();

		/// Apply queued commands, including commands enqueued while flushing.
		///
		bool flush();

		/// Advance the standalone query change window. Systems retain their own windows.
		///
		void clearTrackers();

		/// Hierarchy operations reject stale handles, self-parenting, and cycles.
		///
		bool setParent(Entity _child, Entity _parent);

		///
		bool removeParent(Entity _child);

		///
		Entity getParent(Entity _child) const;

		///
		Entity getFirstChild(Entity _parent) const;

		///
		Entity getNextSibling(Entity _child) const;

		/// Buffered messages are distinct from synchronous observer events.
		///
		template<typename Ty>
		void writeMessage(Ty _value);

		/// Expire messages older than two calls. App invokes this once per frame.
		///
		void updateMessages();

		/// Returns an observer ID. Optional target restricts delivery to one entity.
		///
		template<typename Ty>
		uint32_t addObserver(
			  void (*_fn)(World&, Entity, const Ty&, void*)
			, void* _userData = NULL
			, Entity _target = Entity()
			);

		/// Removing an observer during dispatch takes effect immediately.
		///
		bool removeObserver(uint32_t _id);

		/// Observers may enqueue commands; structural changes are applied by flush().
		///
		template<typename Ty>
		void trigger(const Ty& _event, Entity _target = Entity());

	private:
		friend struct EcsAccess;
		struct Impl;
		Impl* m_impl;
	};

	/// Immediate entity builder. It does not own the entity's lifetime.
	///
	class EntityMut
	{
	public:
		///
		EntityMut(World& _world, Entity _entity);

		///
		Entity id() const;

		///
		operator Entity() const;

		///
		template<typename Ty>
		EntityMut& insert(Ty&& _value);

		///
		template<typename Ty>
		EntityMut& remove();

		///
		template<typename Ty>
		const Ty* get() const;

		///
		template<typename Ty>
		Ty* getMut();

		///
		bool despawn(bool _recursive = true);

		///
		EntityMut& setParent(Entity _parent);

	private:
		World* m_world;
		Entity m_entity;
	};

	/// Commands own captured values until flush or World destruction.
	///
	class Commands
	{
	public:
		///
		Commands(World& _world);

		/// Reserve an ID immediately; the entity becomes visible when the queue is flushed.
		///
		template<typename... ComponentsT>
		EntityCommands spawn(ComponentsT&&... _components);

		///
		EntityCommands entity(Entity _entity);

		/// Queue a callable accepting World&. Move-only captures are supported.
		///
		template<typename FnT>
		void queue(FnT&& _fn);

		///
		void queue(EcsCommandFn _fn);

		///
		template<typename Ty>
		Commands& insertResource(Ty&& _value);

		///
		template<typename Ty>
		Commands& trigger(Ty _event, Entity _target = Entity());

	private:
		World* m_world;
	};

	/// Deferred entity builder. Operations on stale IDs are ignored when applied.
	///
	class EntityCommands
	{
	public:
		///
		EntityCommands(World& _world, Entity _entity);

		///
		Entity id() const;

		///
		template<typename Ty>
		EntityCommands& insert(Ty&& _value);

		///
		template<typename Ty>
		EntityCommands& remove();

		///
		EntityCommands& despawn(bool _recursive = true);

		///
		EntityCommands& setParent(Entity _parent);

	private:
		World* m_world;
		Entity m_entity;
	};

	/// Stable system index within one Schedule.
	///
	struct SystemId
	{
		///
		SystemId();

		///
		explicit SystemId(uint32_t _idx);

		uint32_t idx;
	};

	/// Order and condition configuration for a system or a set. Set labels are user IDs.
	///
	class SystemConfig
	{
	public:
		///
		SystemId id() const;

		///
		SystemConfig& before(SystemId _other);

		///
		SystemConfig& after(SystemId _other);

		///
		SystemConfig& inSet(uint32_t _set);

		/// Order before every member of the specified set.
		///
		SystemConfig& beforeSet(uint32_t _set);

		/// Order after every member of the specified set.
		///
		SystemConfig& afterSet(uint32_t _set);

		/// Conditions are evaluated when the system is reached; skipped systems retain ticks.
		///
		SystemConfig& runIf(EcsRunConditionFn _fn, void* _userData = NULL);

	private:
		friend class  Schedule;
		friend struct EcsAccess;
		SystemConfig(
			  Schedule& _schedule
			, uint32_t _idx
			, bool _set = false
			);
		Schedule* m_schedule;
		uint32_t  m_idx;
		bool      m_set;
	};

	/// Sequential dependency scheduler. Deferred commands flush at dependency boundaries.
	///
	class Schedule
	{
		FROST_CLASS(Schedule
			, NO_COPY
			);

	public:
		///
		explicit Schedule(AllocatorI* _allocator);

		///
		~Schedule();

		/// System functions accept QueryT, ResT, ResMutT, LocalT, Commands, readers/writers,
		///   and World&. World& is exclusive and flushes pending commands before running.
		///
		template<typename... ArgsT>
		SystemConfig addSystems(void (*_fn)(ArgsT...));

		///
		SystemConfig configureSet(uint32_t _set);

		///
		EcsScheduleResult::Enum run(World& _world);

		///
		uint32_t getSystemCount() const;

	private:
		friend struct EcsAccess;
		friend class  SystemConfig;
		struct Impl;
		Impl* m_impl;
	};

	/// FROST plugin interface. App owns plugins and calls build, ready, finish, then cleanup.
	///
	struct FROST_NO_VTABLE PluginI
	{
		///
		virtual ~PluginI() = 0;

		/// Build runs immediately, and may add other plugins.
		///
		virtual void build(App& _app) = 0;

		/// update() waits until every plugin is ready, without blocking the caller.
		///
		virtual bool ready(const App& _app) const;

		///
		virtual void finish(App& _app);

		///
		virtual void cleanup(App& _app);

		/// Unique plugins cannot be added twice.
		///
		virtual bool isUnique() const;

		/// NULL uses C++ type identity; a name overrides duplicate detection.
		///
		virtual const char* getName() const;
	};

	/// App composes a World, schedules, state transitions, and allocator-owned plugins.
	///
	class App
	{
		FROST_CLASS(App
			, NO_COPY
			);

	public:
		///
		explicit App(AllocatorI* _allocator);

		/// Runs shutdown if startup began, then destroys plugins, schedules, and resources.
		///
		~App();

		///
		World& getWorld();

		///
		const World& getWorld() const;

		///
		Schedule& getSchedule(EcsStage::Enum _stage);

		/// Custom schedule label. Invoked explicitly using getSchedule<T>().run(world).
		///
		template<typename LabelT>
		Schedule& getSchedule();

		///
		template<typename... ArgsT>
		SystemConfig addSystems(EcsStage::Enum _stage, void (*_fn)(ArgsT...));

		///
		template<typename Ty, typename... ArgsT>
		App& insertResource(ArgsT&&... _args);

		/// Default-construct once. OnEnter for the initial state runs on the first update.
		///
		template<typename Ty>
		App& initState(Ty _initial);

		/// Condition callback suitable for SystemConfig::runIf.
		///
		template<typename Ty, Ty ValueT>
		static bool inState(const World& _world, void* _userData = NULL);

		///
		template<typename Ty, typename... ArgsT>
		SystemConfig onEnter(Ty _state, void (*_fn)(ArgsT...));

		///
		template<typename Ty, typename... ArgsT>
		SystemConfig onExit(Ty _state, void (*_fn)(ArgsT...));

		/// False for duplicate plugins, or once plugin finishing has begun.
		///
		template<typename PluginT, typename... ArgsT>
		bool addPlugins(ArgsT&&... _args);

		///
		template<typename PluginT>
		bool isPluginAdded() const;

		/// Runs startup once, transitions, frame schedules, and zero or more fixed steps.
		/// Returns false while plugins are pending, after exit, or after a schedule error.
		///
		bool update(double _delta = 0.0);

		/// A runner supplies the platform event loop. The default runner updates once.
		///
		void setRunner(EcsRunnerFn _fn, void* _userData = NULL);

		/// Runs the runner (or one update), then shuts down before returning.
		///
		void run();

		/// Stops updates and runs Shutdown once if startup began, even if startup failed.
		/// Returns false during an update/plugin build or after a shutdown schedule error.
		/// A busy world can be retried; other schedule results are retained on later calls.
		///
		bool shutdown();

		/// Requests exit. The runner should return so run() can perform shutdown.
		/// Manual update loops call shutdown() after their loop, or rely on destruction.
		///
		void exit();

		///
		bool shouldExit() const;

	private:
		friend struct EcsAccess;
		struct Impl;
		Impl* m_impl;
	};

} // namespace frost

#include "inline/ecs.inl"

#endif // FROST_ECS_H_HEADER_GUARD
