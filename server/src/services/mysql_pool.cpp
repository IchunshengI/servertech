#include "services/mysql_pool.hpp"

namespace chat{

mysql_pool::mysql_pool(boost::asio::any_io_executor ex, std::string host, boost::mysql::handshake_params params, config cfg):ex_(ex),\
                        host_(std::move(host)),params_(std::move(params)),cfg_(std::move(cfg)),timer_(ex_)
{
    //idle_conns_.resize(cfg_.max_connections);
}

void mysql_pool::init(){
    boost::asio::spawn(
        boost::asio::bind_executor(
            ex_,
            [this ](boost::asio::yield_context yield) {
                this->run_maintenance(yield);
            }
        )
    );
    start_maintenance();
}
/* 开始连接 */
void mysql_pool::start_maintenance()
{
    timer_.expires_after(std::chrono::seconds(30));
    timer_.async_wait([self = shared_from_this()](boost::system::error_code ec){
        if(!ec){
            boost::asio::spawn(
            boost::asio::bind_executor(
                    self->ex_,
                    [self](boost::asio::yield_context yield) {
                        std::cerr << " 定时启动 " << std::endl;
                        self->run_maintenance(yield);
                    }
                )
            );
        }
        self->start_maintenance();
    });
}
mysql_pool::~mysql_pool()
{
    close();
}

/* 线程池维护 */
void mysql_pool::run_maintenance(boost::asio::yield_context yield){
    // boost::asio::post(strand_,[self = shared_from_this()](){
        const auto now = std::chrono::steady_clock::now();

        {
            std::unique_lock<std::mutex> lock(mutex_);
            /* 清理过期连接 */    
            auto it = std::remove_if(this->idle_conns_.begin(),
                this->idle_conns_.end(),
                [&](const auto& entry){
                    return (now - entry.last_used) > this->cfg_.max_idle_time;
                });
            this->idle_conns_.erase(it, this->idle_conns_.end());
            total_count_ = idle_conns_.size();
        }

        /* 补充最小连接数 */
        while(this->total_count_.load() < cfg_.min_connections){
            this->create_new_connection(yield);
        }
    
}

/* 创建新连接 */
bool mysql_pool::create_new_connection(boost::asio::yield_context yield){
    boost::system::error_code ec;
    connection_type conn(ex_);
    boost::mysql::diagnostics diag;
    ++this->total_count_;
    boost::asio::ip::tcp::resolver resolver(ex_);
    auto endpoints = resolver.async_resolve(host_, boost::mysql::default_port_string, yield[ec]);
    if (ec) {
        std::cerr << "async_resolve error " << std::endl;
        // if (handler) handler(ec, std::move(conn)); /* 这里必须用std::move，内部禁用了拷贝构造函数 */
        return false;  
    }
    ec.clear();
    // /* 连接mysql */
    conn.async_connect(*endpoints.begin(), params_, diag,yield[ec]);
    if(ec){
        std::cerr << "mysql async_connect error " << std::endl;
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if(total_count_.load() < cfg_.max_connections){
        this->idle_conns_.push_back({std::move(conn), std::chrono::steady_clock::now()});
        return true;
    }else{
        conn.close();    
        total_count_.fetch_sub(1); 
        return false;
    }
}


// 协程专用接口
 std::pair<boost::system::error_code, mysql_pool::connection_type>  mysql_pool::get_connection(boost::asio::yield_context yield){
    boost::system::error_code ec;
    connection_type conn(ex_);

    if (!idle_conns_.empty()) {
            // 直接返回空闲连接
            std::unique_lock<std::mutex> lock(mutex_);
            conn = std::move(idle_conns_.front().conn);
            idle_conns_.pop_front();
            ec.clear();
    }
    // 通过post确保在strand_中执行
    else if (total_count_.load() < cfg_.max_connections) {
            // 创建新连接
            if(create_new_connection(yield)){
                return get_connection(yield);    
            }else{
                ec = boost::asio::error::operation_aborted;
                std::cerr << "创建新连接失败 " << std::endl;
            }
    } else {
        // 连接池已满
        ec = boost::asio::error::operation_aborted;
    }

    return {ec, std::move(conn)};
}

void mysql_pool::return_connection(connection_type conn){

        std::unique_lock<std::mutex> lock(mutex_);
        this->idle_conns_.push_back({std::move(conn), std::chrono::steady_clock::now()});

}

void mysql_pool::close(){
    // boost::asio::post(strand_,[self = shared_from_this()](){
    //     self->closed_ = true;
        this->idle_conns_.clear();
        this->timer_.cancel();
    // });
}

}