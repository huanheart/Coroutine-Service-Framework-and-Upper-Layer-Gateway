#include"../memorypool/memorypool.hpp"
#include <iostream>
#include<list>
#include<unordered_map>
#include<climits> // integer limit definitions
#include<cstddef> // size_t and related definitions
using namespace std;

// Lightweight find cache and LRU cache with push/pop/find/update interfaces

#define f_end -1

template<typename P>
class BackNode
{
public:
    int index;
    P passwd;
};


template<typename T,typename Alloc=std::allocator<T> >
class FindCache    //ʵ������ɾ���ģ��顣�Լ�ɾ�������ڴ�
{
private:
    template<typename U>
    class FindNode
    {
    public:
        FindNode(U key,U value){
            this->key=key;
            this->value=value;
            next=nullptr;     //�������û��������������ֻ����Ԫ�����(���߿�����һ���������ڲ������ռ�Ҳ�������������಻���ʵ���)
        }
        FindNode(){
            next=nullptr;
        }
    public:
        U key;
        U value;
        FindNode * next;
    };

private:
    void clear();
public:
    typedef FindNode<T> Node;
    typedef typename Alloc::template rebind<Node>::other allocator;
    //Ĭ�Ϲ���
    FindCache(){
        aider=allocator_.allocate(1); //����һ��T���ʹ�С�Ŀռ�
        allocator_.construct(aider,Node() ); //Ȼ������乹�캯��
        head=nullptr;     //��������ѡ��ʹ��ͷ�巨
        size_=0;
    }
    ~FindCache(){
        clear();
    }

    bool push(T key,T value);

    bool pop(T key,T value);

    bool update(T key,T value); //���ﱣ֤key�ǲ��ܱ��ĵ�

    BackNode<T> find(T key);  //����һ���±�

    long long get_size(){
        return size_;
    }
private:
    allocator allocator_;
    Node * head;    // head node
    Node * aider;   // dummy aide node (aider->next is the first element)
    long long size_; // element count
};

template<typename T,typename Alloc>
void FindCache<T,Alloc>::clear()
{
    Node * temp=aider->next;

    while(temp)
    {
        Node * temp_2=temp;
        temp=temp->next;
        aider->next=temp;
        cout<<temp_2->key<<' '<<temp_2->value<<endl;
        //�ȵ������������������ͷ��ڴ�
        allocator_.destroy(temp_2);
        allocator_.deallocate(temp_2,1);
        
    }
    //����ͷŸ������
    allocator_.destroy(aider);
    allocator_.deallocate(aider,1);

}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::push(T key,T value)  //����Ԫ��
{
    BackNode<T> back;
    back=find(key);
    if(back.index!=f_end) //���������������ͬkey��Ԫ�أ�keyֵΨһ
        return false;
    Node *newnode=allocator_.allocate(1); //����һ��T���ʹ�С�Ŀռ�
    allocator_.construct(newnode,Node(key,value) ); //Ȼ������乹�캯��
    //���ڿ�ʼ���в�������
    newnode->next=head;
    aider->next=newnode;
    head=newnode;
    size_++;
    return true;
}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::pop(T key,T value) //Ҫȥ���Ҷ�Ӧ��ֵ�Ƿ�����ͬ�Ľ��
{
    //�Ӹ���������
    Node * temp=aider->next;
    Node * last_temp=aider;
    while(temp){
        if(temp->key==key&&temp->value==value){
            //����ɾ��
            last_temp->next=temp->next;
            size_--;
            //������е�ԭ��head������ָ����������һ����㣬��ô���ɾ����head��������Ҫ��һЩ����������head������ָ��
            //����head����Ұָ��
            if(head==temp){ 
                head=last_temp->next;
            }
            //�ȵ������������������ͷ��ڴ�
            allocator_.destroy(temp);            
            allocator_.deallocate(temp,1);
            return true;
        }
        temp=temp->next;
        last_temp=last_temp->next;
    }

    return false; //˵��û���ҵ�
}


template<typename T,typename Alloc>
BackNode<T> FindCache<T,Alloc>::find(T key)
{
    Node * temp=aider->next;
    int index=1; //ע�⣺��1��ʼ��ʾ��һ��Ԫ��
    BackNode<T> back;
    while(temp)
    {
        if(temp->key==key){
            back.index=index;
            back.passwd=temp->value;
            return back;
        }
        temp=temp->next;
        index++;
    }
    back.index=f_end;
    return back;
}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::update(T key,T value) 
{
    Node* temp=aider->next;
    while(temp){
        if(temp->key==key){
            temp->value=value;
            return true;
        }
        temp=temp->next;
    }
    return false;
};


//�����������ݽṹΪ��Ȩ����LRU
//���ڲ����˺���������ݽṹ���滻�����������Ļ������ݽṹ
template<typename T,typename U,typename Alloc=std::allocator<T> >
class LRUCache {
private:
    template<typename K,typename P>
    class LruNode {
    public:
        LruNode(K key,P value) {
            this->key = key;
            this->value = value;
        }
    public:
        K key;
        P value;
    };
    //������Ϊ-1��ʱ�򣬱�ʾ������LRU�����С,�ж��ٴ����

private:
    void clear();
public:
    typedef LruNode<T,U> Node;
    typedef LruNode<T,U>* NodePtr;
    // 使用 allocator::rebind 获取 Node 的分配器类型
    typedef typename Alloc::template rebind<Node>::other allocator;
    // �� C++11 ��֮���Ƽ�ʹ�� std::allocator_traits ������ rebind��
    // typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Node> allocator;

    LRUCache() {
        capacity=-1;
    }
    LRUCache(int capacity) {
        this->capacity = capacity;
    }
    ~LRUCache() {
        clear();
    }
    void push(T key,U value);
    pair<bool,U> get(T key);
private:
    int capacity;
    unordered_map<T,typename list<NodePtr>::iterator>  map;
    list<NodePtr> my_list;
    allocator allocator_;
};


template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::push(T key, U value) {
    //�Ȳ鿴�Ƿ��Ѿ����ڣ�������ڣ���ô�޸�ԭ�ȵ��Ǹ�ֵ
    if (map.find(key)!=map.end()) {
        my_list.splice(my_list.begin(),my_list,map[key]);
        my_list.front()->value=value;
        return ;
    }
    // 当容量受限并达到上限时，淘汰末尾元素
    if (capacity!=-1 && my_list.size()>=capacity) {
        //��ȡ���һ��Ԫ�أ���������
        map.erase(my_list.back()->key);
        NodePtr temp=my_list.back();
        //�ȵ�����������
        allocator_.destroy(temp);
        //���ͷ��ڴ�
        allocator_.deallocate(temp,1);
        my_list.pop_back();
    }
    //�ȷ����ڴ�
    NodePtr newnode=allocator_.allocate(1);
    //Ȼ������乹�캯��
    allocator_.construct(newnode,Node(key,value) );
    //Ȼ����������뵽list��
    my_list.emplace_front(newnode);
    map[key]=my_list.begin();
}

template<typename T, typename U, typename Alloc>
pair<bool,U> LRUCache<T,U,Alloc> :: get(T key) {
    if (map.find(key) == map.end()) {
        //���Uһ����Ҫ�ղι��캯��,�����ڲ�LRU�������ݽṹ�ᱨ��
        return pair<bool,U>(false,U{} );
    }
    my_list.splice( my_list.begin(),my_list,map[key]);
    return pair<bool,U>(true,my_list.front()->value);
}

template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::clear() {
    //��Ҫ�������е�ָ��Ԫ��ȫ������
    map.clear();
    while (my_list.size() ) {
        NodePtr temp=my_list.front();
        cout<<"Node delete "<<temp->key<<" "<<temp->value<<endl;
        //�ȵ�����������
        allocator_.destroy(temp);
        //���ͷ��ڴ�
        allocator_.deallocate(temp,1);
        my_list.pop_front();
    }
}





//end
