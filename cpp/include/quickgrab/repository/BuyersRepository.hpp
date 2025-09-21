#pragma once

#include "quickgrab/model/Buyer.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"

#include <vector>

namespace quickgrab::repository {

class BuyersRepository {
public:
    explicit BuyersRepository(MySqlConnectionPool& pool);

    std::vector<model::Buyer> findAll();

private:
    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository

