#if INTERFACE
#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/HashMap.h>

class GarbageCollector {
public:
    static vector<Obj*> GrayList;
    static Obj*         RootObject;

    static size_t       NextGC;
    static size_t       GarbageSize;
    static double       MaxTimeAlotted;

    static bool         Print;
};
#endif

#include <Engine/Bytecode/GarbageCollector.h>

#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Scene.h>

#define GC_HEAP_GROW_FACTOR 2

vector<Obj*> GarbageCollector::GrayList;
Obj*         GarbageCollector::RootObject;

size_t       GarbageCollector::NextGC = 1024;
size_t       GarbageCollector::GarbageSize = 0;
double       GarbageCollector::MaxTimeAlotted = 1.0; // 1ms

bool         GarbageCollector::Print = false;

PUBLIC STATIC void GarbageCollector::Collect() {
    GrayList.clear();

    // Mark threads (should lock here for safety)
    for (Uint32 t = 0; t < BytecodeObjectManager::ThreadCount; t++) {
        VMThread* thread = BytecodeObjectManager::Threads + t;
        // Mark stack roots
        for (VMValue* slot = thread->Stack; slot < thread->StackTop; slot++) {
            GrayValue(*slot);
        }
        // Mark frame functions
        for (Uint32 i = 0; i < thread->FrameCount; i++) {
            GrayObject(thread->Frames[i].Function);
        }
    }

    // Mark global roots
    GrayHashMap(BytecodeObjectManager::Globals);

    // Mark static objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;

        BytecodeObject* bobj = (BytecodeObject*)ent;
        GrayObject(bobj->Instance);
        GrayHashMap(bobj->Properties);
    }
    // Mark dynamic objects
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;

        BytecodeObject* bobj = (BytecodeObject*)ent;
        GrayObject(bobj->Instance);
        GrayHashMap(bobj->Properties);
    }

    // Mark functions
    for (size_t i = 0; i < BytecodeObjectManager::AllFunctionList.size(); i++) {
        GrayObject(BytecodeObjectManager::AllFunctionList[i]);
    }

    // Traverse references
    for (size_t i = 0; i < GrayList.size(); i++) {
        BlackenObject(GrayList[i]);
    }

    // Collect the white objects
    Obj** object = &GarbageCollector::RootObject;
    while (*object != NULL) {
        if (!((*object)->IsDark)) {
            // This object wasn't reached, so remove it from the list and
            // free it.
            Obj* unreached = *object;
            *object = unreached->Next;

            BytecodeObjectManager::FreeValue(OBJECT_VAL(unreached));
        }
        else {
            // This object was reached, so unmark it (for the next GC) and
            // move on to the next.
            (*object)->IsDark = false;
            object = &(*object)->Next;
        }
    }

    GarbageCollector::NextGC = GarbageCollector::GarbageSize * GC_HEAP_GROW_FACTOR;
}

PUBLIC STATIC void GarbageCollector::GrayValue(VMValue value) {
    if (!IS_OBJECT(value)) return;
    GrayObject(AS_OBJECT(value));
}
PUBLIC STATIC void GarbageCollector::GrayObject(void* obj) {
    if (obj == NULL) return;

    Obj* object = (Obj*)obj;
    if (object->IsDark) return;

    object->IsDark = true;

    GrayList.push_back(object);
}
PUBLIC STATIC void GarbageCollector::GrayHashMapItem(Uint32, VMValue value) {
    GrayValue(value);
}
PUBLIC STATIC void GarbageCollector::GrayHashMap(void* pointer) {
    if (!pointer) return;
    ((HashMap<VMValue>*)pointer)->ForAll(GrayHashMapItem);
}

PUBLIC STATIC void GarbageCollector::BlackenObject(Obj* object) {
    switch (object->Type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            GrayValue(bound->Receiver);
            GrayObject(bound->Method);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            GrayObject(klass->Name);
            GrayHashMap(klass->Methods);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            GrayObject(function->Name);
            for (size_t i = 0; i < function->Chunk.Constants->size(); i++) {
                GrayValue((*function->Chunk.Constants)[i]);
            }
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            GrayObject(instance->Class);
            GrayHashMap(instance->Fields);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            for (size_t i = 0; i < array->Values->size(); i++) {
                GrayValue((*array->Values)[i]);
            }
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            map->Values->ForAll([](Uint32, VMValue v) -> void {
                GrayValue(v);
            });
            break;
        }
        // case OBJ_NATIVE:
        // case OBJ_STRING:
        default:
            // No references
            break;
    }
}

PUBLIC STATIC void GarbageCollector::RemoveWhiteHashMapItem(Uint32, VMValue value) {
    // seems in the craftinginterpreters book this removes the ObjString used
    // for hashing, but we don't use that, so...
}
PUBLIC STATIC void GarbageCollector::RemoveWhiteHashMap(void* pointer) {
    if (!pointer) return;
    ((HashMap<VMValue>*)pointer)->ForAll(RemoveWhiteHashMapItem);
}