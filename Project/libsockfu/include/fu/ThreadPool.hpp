/*

- BEGIN LICENSE NOTICE -

Copyright (c) 2012 Jakob Progsch, Václav Zeman

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.

- END LICENCE NOTICE -

THIS IS AN ALTERED SOURCE VERSION!!!

*/

#ifndef FU_THREAD_POOL_HPP
#define FU_THREAD_POOL_HPP

#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <algorithm>
#include <stdexcept>

namespace fu
{
    template <typename T, template <typename U, typename V = std::allocator<U>> typename W>
    class GuardedSequence
    {
        public:
            typedef W<T*> Container;
            GuardedSequence(bool isResponsible = false) : isResponsible(isResponsible) {}
            GuardedSequence(std::initializer_list<T*> items, bool isResponsible = false) :
                isResponsible(isResponsible)
            {
                for (typename std::initializer_list<T*>::iterator iter = items.begin(); iter != items.end(); ++iter)
                {
                    push(iter->second);
                }
            }
            virtual ~GuardedSequence()
            {
                purge();
            }
            std::string getSerial(std::function<std::string(T*)> conversion) const
            {
                std::string serial = "( ";
                for (unsigned int i = 0; i != getSize(); ++i)
                {
                    if (i != 0)
                    {
                        serial += ", ";
                    }
                    serial += conversion(getAt(i));
                }
                return serial;
            }
            void absorb(const GuardedSequence* target)
            {
                for (unsigned int i = 0; i != target->getSize(); ++i)
                {
                    push(target->getAt(i));
                }
            }
            void absorb(GuardedSequence* target, bool doDelete)
            {
                for (unsigned int i = 0; i != target->getSize(); ++i)
                {
                    push(target->getAt(i));
                }
                if (doDelete)
                {
                    target->setIsResponsible(false);
                    delete target;
                }
            }
            void sort(std::function<bool(T*,T*)> comparison)
            {
                std::sort(container.begin(), container.end(), comparison);
            }
            bool empty() const
            {
                return container.empty();
            }
            unsigned int getSize() const
            {
                return container.size();
            }
            T* getAt(unsigned int index) const
            {
                if (index >= container.size())
                {
                    return nullptr;
                }
                return container[index];
            }
            T* getFirst() const
            {
                if (container.empty())
                {
                    return nullptr;
                }
                return container.front();
            }
            T* getLast() const
            {
                if (container.empty())
                {
                    return nullptr;
                }
                return container.back();
            }
            bool insert(T* value, unsigned int index)
            {
                if (index > container.size())
                {
                    return false;
                }
                if (subinsert(value))
                {
                    container.insert(container.begin()+index, value);
                    onAdd(value);
                    return true;
                }
                return false;
            }
            bool erase(T* value)
            {
                for (unsigned int i = 0; i != container.size(); ++i)
                {
                    if (container[i] == value)
                    {
                        if (suberase(value))
                        {
                            onRemove(value);
                            if (isResponsible)
                            {
                                delete value;
                            }
                            container.erase(container.begin()+i);
                            return true;
                        }
                        break;
                    }
                }
                return false;
            }
            bool eraseAt(unsigned int index)
            {
                if (index >= container.size())
                {
                    return false;
                }
                T* temp = container[index];
                if (suberase(temp))
                {
                    onRemove(temp);
                    if (isResponsible)
                    {
                        delete temp;
                    }
                    container.erase(container.begin()+index);
                    return true;
                }
                return false;
            }
            bool push(T* value)
            {
                if (subpush(value))
                {
                    container.push_back(value);
                    onAdd(value);
                    return true;
                }
                return false;
            }
            bool pop()
            {
                T* temp = container.back();
                if (subpop(temp))
                {
                    onRemove(temp);
                    if (isResponsible)
                    {
                        delete temp;
                    }
                    container.pop_back();
                    return true;
                }
                return false;
            }
            bool purge()
            {
                if (subpurge())
                {
                    while (!container.empty())
                    {
                        pop();
                    }
                    return true;
                }
                return false;
            }
            template <typename ...Args>
            bool add(Args&& ...args)
            {
                return push(new T(std::forward<Args>(args)...));
            }
            bool getIsResponsible() const
            {
                return isResponsible;
            }
            void setIsResponsible(bool isResponsible)
            {
                this->isResponsible = isResponsible;
            }
            const Container& getContainer() const
            {
                return container;
            }
        protected:
            virtual void onAdd(T* value) {}
            virtual void onRemove(T* value) {}
            virtual bool subinsert(T* value)
            {
                return true;
            }
            virtual bool suberase(T* value)
            {
                return true;
            }
            virtual bool subpush(T* value)
            {
                return true;
            }
            virtual bool subpop(T* value)
            {
                return true;
            }
            virtual bool subpurge()
            {
                return true;
            }
            bool isResponsible;
            Container container;
    };

    class Task
    {
        public:
            Task(unsigned int priority, const std::string& name) : priority(priority), name(name), task([](){}) {}
            unsigned int getPriority() const
            {
                return priority;
            }
            const std::string& getName() const
            {
                return name;
            }
            std::function<void()> task;
            bool result;
        private:
            unsigned int priority;
            std::string name;
    };

    template <typename T>
    class ThreadPoolBase : public GuardedSequence<T, std::vector>
    {
        public:
            ThreadPoolBase(unsigned int threadCount) :
                GuardedSequence<T,std::vector>(true),
                isStopped(false)
            {
                for (unsigned int i = 0; i != threadCount; ++i)
                {
                    workers.emplace_back([this]
                    {
                        bool isWorking = true;
                        while (isWorking)
                        {
                            Task* task = nullptr;
                            {
                                std::chrono::milliseconds timespan(100);
                                std::unique_lock<std::mutex> lock(this->queueMutex);
                                this->queueCondition.wait_for(lock, timespan);
                                if (!this->isStopped)
                                {
                                    task = this->getLast();
                                    bool temp = this->getIsResponsible();
                                    this->setIsResponsible(false);
                                    this->pop();
                                    this->setIsResponsible(temp);
                                }
                                else
                                {
                                    isWorking = false;
                                }
                            }
                            if (task != nullptr)
                            {
                                //std::cout << task->getName() << std::endl;
                                task->task();
                                bool result = task->result;
                                delete task;
                                if (!result)
                                {
                                    std::unique_lock<std::mutex> lock(this->queueMutex);
                                    for (unsigned int i = 0; i != this->workers.size(); ++i)
                                    {
                                        if (std::this_thread::get_id() == this->workers[i].get_id())
                                        {
                                            //std::cout << "hello" << std::endl;
                                            this->workers[i].detach();
                                            this->workers.erase(this->workers.begin()+i);
                                            if ((this->workers.empty()) && (this->empty()))
                                            {
                                                task = nullptr;
                                            }
                                            break;
                                        }
                                    }
                                }
                                if (task == nullptr)
                                {
                                    delete this;
                                    isWorking = false;
                                }
                            }
                        }
                    });
                }
            }
            virtual ~ThreadPoolBase()
            {
                //std::cout << "pool" << std::endl;
                std::unique_lock<std::mutex> lock(queueMutex);
                isStopped = true;
                queueCondition.notify_all();
                for (unsigned int i = 0; i != workers.size(); ++i)
                {
                    if (std::this_thread::get_id() == workers[i].get_id())
                    {
                        workers[i].detach();
                    }
                    else
                    {
                        workers[i].join();
                    }
                    //std::cout << i << std::endl;
                }
                workers.clear();
                this->purge();
            }
            template<typename Func, typename... Args>
            auto enqueue(unsigned int priority, const std::string& name, Func&& func, Args&&... args)
                -> std::future<typename std::result_of<Func(bool*, Args...)>::type>
            {
                using returnType = typename std::result_of<Func(bool*, Args...)>::type;
                Task* task = new T(priority, name);
                auto binding = std::bind(std::forward<Func>(func), std::forward<bool*>(&task->result), std::forward<Args>(args)...);
                auto package = std::make_shared<std::packaged_task<returnType()>>(binding);
                task->task = [=](){try{(*package)();}catch(...){}};
                std::future<returnType> result = package->get_future();
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    if (isStopped)
                    {
                        std::string error = "ERROR: Cannot enqueue on a Thread Pool which has already been shut down!";
                        throw std::runtime_error(error);
                    }
                    this->push(task);
                    queueCondition.notify_one();
                }
                return result;
            }
            void onAdd(T* value)
            {
                this->sort([](T* left, T* right){return (left->getPriority() > right->getPriority());});
            }
        private:
            std::vector<std::thread> workers;
            std::mutex queueMutex;
            std::condition_variable queueCondition;
            bool isStopped;
    };

    typedef ThreadPoolBase<Task> ThreadPool;
}

#endif
