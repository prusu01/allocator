#include <bits/stdc++.h>


using namespace std;
#define add_to_p(P, L) P = (void *)((char *)P + L)

template <class T>
class my_allocator{
private:
    int objs;
    

    union slot{
        slot *next; //next free block if this is free, null by default
        T data;
    };
    slot* memory;
    slot *next_unbuilt;
    slot *head_freed;
    slot *end;

public:
    my_allocator(int nb){
        size_t size  = nb * sizeof(slot);
        next_unbuilt = (slot *)malloc(size);
        memory = next_unbuilt;
        end = next_unbuilt + nb;
        objs = 0;
        head_freed = nullptr;
    }

    ~my_allocator(){
        free(memory);
    }

    T * allocate(){
        cout<<"allocate"<<endl;
        slot *to_use = nullptr;

        if(head_freed){
            to_use = head_freed;
            head_freed = head_freed->next; 
        }
        else if(next_unbuilt < end){
            to_use = next_unbuilt;
            next_unbuilt++;
        }
        else{
            cout<<"full"<<endl;
            return nullptr;
        }

        T *obj = new(static_cast<void*>(&to_use->data))T();
        objs++;
        return obj;
    }

    void destroy(T * obj){
        cout<<"destroy"<<endl;
        slot *recvd = reinterpret_cast<slot*>(obj);
        if(recvd < memory || recvd>= next_unbuilt){
            cout<<"memory from other pool"<<endl;
            return;
        }

        obj->~T();
        objs --;

        
        recvd->next = nullptr;
        if(!head_freed){
            head_freed = recvd;
        }
        else{
            recvd->next = head_freed;
            head_freed = recvd;
        }

    }

    int get_object_nb(){
        return objs;
    }

};
