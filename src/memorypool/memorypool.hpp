#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP


#include<climits> // integer limit definitions (e.g., INT_MAX)
#include<cstddef> // size_t and related definitions

template<typename T,size_t BlockSize=4096>
class MemoryPool
{
public:
    typedef T* pointer;
    //����rebind<U>::other�ӿ�
    template<typename U> struct rebind{
        using other=MemoryPool<U>;
    };

    //��ʽ˵���ú��������ܳ����쳣,���ӿɶ���
    MemoryPool() noexcept{
        currentBlock_=nullptr;
        currentSlot_=nullptr;
        lastSlot_=nullptr;
        freeSlots_=nullptr;
    }

    ~MemoryPool()noexcept{
        //ѭ�����������ڴ���е��ڴ�����
        slot_pointer_ curr=currentBlock_;
        while(curr!=nullptr){
            slot_pointer_ prev=curr->next;
            operator delete(reinterpret_cast<void*>(curr) );
            curr=prev;
        }
    }

    //ͬһ��ʱ��ֻ�ܷ���һ������,n��hint�ᱻ����
    pointer allocate(size_t n=1,const T*hint=0 );

    //����ָ��pָ����ڴ������
    void deallocate(pointer p,size_t n=1);

    //���ù��캯��
    template<typename U,typename ...Args>
    void construct(U*p,Args&&...args);

    //�����ڴ���еĶ�����ʽ������������
    template<typename U>
    void destroy(U* p){
        p->~U();
    }



private:


    // Slot holds either an object or a next pointer for free-list
    union Slot_
    {
        T element; 
        Slot_ * next;
    };

    

    //����ָ��
    using data_pointer_=char*;
    //�����
    using slot_type_=Slot_;
    //�����ָ��
    using slot_pointer_=Slot_*;

    // current memory block
    slot_pointer_ currentBlock_;
    // next available slot in current block
    slot_pointer_ currentSlot_;
    // last valid slot in current block
    slot_pointer_ lastSlot_;
    // free-list head
    slot_pointer_ freeSlots_;

    //��̬���ԣ��ڱ����ʱ��ȥ�����жϣ�����assert���Ƕ�̬�ģ�������ʱ�����ж�
    static_assert(BlockSize>=2*sizeof(slot_type_),"BlockSize too small " );
};

// placement new construct
template<typename T,size_t BlockSize>
template<typename U, typename... Args>
void MemoryPool<T,BlockSize>::construct(U* p, Args&&... args) {
    // construct object in pre-allocated memory by placement new
    new(p) U(std::forward<Args>(args)...);
}


// deallocate given pointer into free-list
template<typename T,size_t BlockSize>
void MemoryPool<T,BlockSize>::deallocate(pointer p,size_t n)
{
    if(p!=nullptr){
        reinterpret_cast<slot_pointer_>(p)->next=freeSlots_;
        freeSlots_=reinterpret_cast<slot_pointer_>(p);
    }

}

template<typename T,size_t BlockSize>
typename MemoryPool<T,BlockSize>::pointer MemoryPool<T,BlockSize>::allocate(size_t n,const T*hint)
{
    // if free-list has slots, reuse
    if(freeSlots_!=nullptr){
        pointer result=reinterpret_cast<pointer>(freeSlots_);
        freeSlots_=freeSlots_->next;
        return result;
    }else{ 
        // else ensure current block has capacity
        if(currentSlot_>=lastSlot_){ 
            // allocate a new block and chain previous
            data_pointer_ newBlock=reinterpret_cast<data_pointer_>(operator new(BlockSize) );
            reinterpret_cast<slot_pointer_>(newBlock)->next=currentBlock_;
            currentBlock_=reinterpret_cast<slot_pointer_>(newBlock);
            // compute first valid slot aligned for Slot_ type
            data_pointer_ body=newBlock+sizeof(slot_pointer_);
            uintptr_t result=reinterpret_cast<uintptr_t>(body);
            size_t bodyPadding=( alignof(slot_type_) )-result%alignof(slot_type_);
            currentSlot_=reinterpret_cast<slot_pointer_>(body+bodyPadding);
            lastSlot_=reinterpret_cast<slot_pointer_>(newBlock+BlockSize-sizeof(slot_type_)+1 );
        }
        return reinterpret_cast<pointer>(currentSlot_++);

    }

}


#endif
