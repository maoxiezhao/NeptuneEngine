#include "object.h"
#include "core\engine.h"
#include "core\utils\profiler.h"
#include "core\platform\timer.h"

namespace VulkanTest
{
	namespace
	{
		struct ObjectServiceDataImpl
		{
			Mutex newAddedMutex;
			Mutex toRemovedMutex;
			std::unordered_map<Object*, F32> newAdded;
			std::unordered_map<Object*, F32> toRemoved;
			bool initialized = false;
		};
		ObjectServiceDataImpl gImpl;
	}

	class ObjectServiceImpl : public EngineService
	{
	public:
		ObjectServiceImpl() :
			EngineService("ObjectService"),
			lastUpdate(0.0f)
		{}

		bool Init() override
		{
			lastUpdate = timer.GetTimeSinceStart();
			initialized = true;
			return true;
		}

		void LateUpdate() override
		{
			PROFILE_FUNCTION();
			F32 now = timer.GetTimeSinceStart();
			F32 dt = now - lastUpdate;
			ObjectService::Flush(dt);
			lastUpdate = now;
		}

		void Uninit() override
		{
			ObjectService::Flush(0);

			// Remove remaining objects
			{
				ScopedMutex lock(gImpl.toRemovedMutex);
				for (auto it = gImpl.toRemoved.begin(); it != gImpl.toRemoved.end(); it++)
				{
					auto obj = it->first;
					obj->Delete();
				}
				gImpl.toRemoved.clear();
			}

			initialized = false;
		}

	private:
		Timer timer;
		F32 lastUpdate;
	};
	ObjectServiceImpl ObjectServiceImplInstance;

	Object::~Object() = default;

	void Object::DeleteObjectNow()
	{
		ObjectService::Dereference(this);
		Delete();
	}

	void Object::DeleteObject(F32 timeToRemove)
	{
		ObjectService::Add(this, timeToRemove);
	}

	void ObjectService::Dereference(Object* obj)
	{
		// Move objects from  newAdded to toRemoved
		{
			ScopedMutex lock(gImpl.newAddedMutex);
			gImpl.newAdded.erase(obj);
		}
		// Try to delete objects from toRemoved
		{
			ScopedMutex lock(gImpl.toRemovedMutex);
			gImpl.toRemoved.erase(obj);
		}
	}

	void ObjectService::Add(Object* obj, F32 time)
	{
		ScopedMutex lock(gImpl.newAddedMutex);
		obj->markedToDelete = true;
		gImpl.newAdded[obj] = time;
	}

	void ObjectService::Flush(F32 dt)
	{
		// Move objects from  newAdded to toRemoved
		{
			ScopedMutex lock(gImpl.newAddedMutex);
			for (auto kvp : gImpl.newAdded)
				gImpl.toRemoved[kvp.first] = kvp.second;
			gImpl.newAdded.clear();
		}

		// Try to delete objects from toRemoved
		{
			ScopedMutex lock(gImpl.toRemovedMutex);
			for (auto it = gImpl.toRemoved.begin(); it != gImpl.toRemoved.end();)
			{
				auto obj = it->first;
				F32 time = it->second - dt;
				if (time < 0.001f)
				{
					it = gImpl.toRemoved.erase(it);
					obj->Delete();
				}
				else
				{
					it->second = time;
					it++;
				}
			}
		}
		
		// Process newAdded objects
		while (HasNewObjectsForFlush())
		{
			// Move objects from  newAdded to toRemoved
			{
				ScopedMutex lock(gImpl.newAddedMutex);
				for (auto kvp : gImpl.newAdded)
					gImpl.toRemoved[kvp.first] = kvp.second;
				gImpl.newAdded.clear();
			}

			// Try to delete objects from toRemoved
			{
				ScopedMutex lock(gImpl.toRemovedMutex);
				for (auto it = gImpl.toRemoved.begin(); it != gImpl.toRemoved.end();)
				{
					auto obj = it->first;
					if (it->second < 0.001f)
					{
						it = gImpl.toRemoved.erase(it);
						obj->Delete();
					}
					else
					{
						it++;
					}
				}
			}
		}
	}

	bool ObjectService::HasNewObjectsForFlush()
	{
		ScopedMutex lock(gImpl.newAddedMutex);
		return !gImpl.newAdded.empty();
	}
}