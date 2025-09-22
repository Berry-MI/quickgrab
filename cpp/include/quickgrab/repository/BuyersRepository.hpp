#pragma once

#include "quickgrab/model/Buyer.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"

#include <optional>
#include <string>
#include <vector>

namespace quickgrab::repository {

class BuyersRepository {
public:
    explicit BuyersRepository(MySqlConnectionPool& pool);

    std::vector<model::Buyer> findAll();
    std::optional<model::Buyer> findByUsername(const std::string& username);

private:
    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository

