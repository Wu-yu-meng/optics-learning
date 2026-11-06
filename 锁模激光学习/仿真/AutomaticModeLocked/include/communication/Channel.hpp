#pragma once
#include <deque>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

//FIFO广播通道，每个订阅者独立保存未接收的消息，共享同一份只读数据。
template<typename T>
class Channel
{
public:
    using DataPtr = std::shared_ptr<const T>;
    using MessageQueue = std::deque<DataPtr>;

private:
    struct SubscriberState
    {
        std::mutex mutex;
        MessageQueue messages;
    };

public:
    class Publisher
    {
        friend class Channel<T>;

    public:
        void publish(const DataPtr &data) const
        {
            m_channel.publish(data);
        }

    private:
        explicit Publisher(Channel<T> &channel)
            : m_channel(channel)
        {
        }

        Channel<T> &m_channel;
    };

    class Subscriber
    {
        friend class Channel<T>;

    public:
        Subscriber(const Subscriber&) = delete;
        Subscriber& operator=(const Subscriber&) = delete;
        Subscriber(Subscriber&&) = default;
        Subscriber& operator=(Subscriber&&) = default;

        //一次取出自己当前排队的全部消息，保留FIFO顺序，没有消息时返回空队列。
        MessageQueue receive()
        {
            MessageQueue messages;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                messages.swap(m_state->messages);//交换后新发布的消息留在订阅队列中，供下次接收。
            }
            return messages;
        }

    private:
        explicit Subscriber(std::shared_ptr<SubscriberState> state)
            : m_state(std::move(state))
        {
        }

        std::shared_ptr<SubscriberState> m_state;
    };

    Channel() = default;
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    Publisher create_publisher()
    {
        return Publisher(*this);
    }

    //只接收订阅者创建之后发布的消息。
    Subscriber create_subscriber()
    {
        std::shared_ptr<SubscriberState> state = std::make_shared<SubscriberState>();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.emplace_back(state);
        return Subscriber(std::move(state));
    }

    void publish(const DataPtr &data)
    {
        //空指针表示没有消息，不进入队列。
        if (!data)
        {
            return;
        }

        //串行投递，保证各订阅者看到相同的发布顺序。
        std::lock_guard<std::mutex> lock(m_mutex);
        typename std::vector<std::weak_ptr<SubscriberState>>::iterator subscriber = m_subscribers.begin();
        while (subscriber != m_subscribers.end())
        {
            std::shared_ptr<SubscriberState> state = subscriber->lock();
            if (!state)
            {
                subscriber = m_subscribers.erase(subscriber);
                continue;
            }

            {
                std::lock_guard<std::mutex> subscriber_lock(state->mutex);
                state->messages.push_back(data);
            }
            ++subscriber;
        }
    }

private:
    std::mutex m_mutex;
    std::vector<std::weak_ptr<SubscriberState>> m_subscribers;
};
