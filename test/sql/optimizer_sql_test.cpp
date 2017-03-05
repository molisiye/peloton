//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// optimizer_sql_test.cpp
//
// Identification: test/sql/optimizer_sql_test.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "sql/testing_sql_util.h"
#include "catalog/catalog.h"
#include "common/harness.h"
#include "executor/create_executor.h"
#include "optimizer/optimizer.h"
#include "optimizer/simple_optimizer.h"
#include "planner/create_plan.h"

namespace peloton {
namespace test {

class OptimizerSQLTests : public PelotonTest {};

void CreateAndLoadTable() {
  // Create a table first
  TestingSQLUtil::ExecuteSQLQuery(
      "CREATE TABLE test(a INT PRIMARY KEY, b INT, c INT);");

  // Insert tuples into table
  TestingSQLUtil::ExecuteSQLQuery("INSERT INTO test VALUES (1, 22, 333);");
  TestingSQLUtil::ExecuteSQLQuery("INSERT INTO test VALUES (2, 11, 000);");
  TestingSQLUtil::ExecuteSQLQuery("INSERT INTO test VALUES (3, 33, 444);");
  TestingSQLUtil::ExecuteSQLQuery("INSERT INTO test VALUES (4, 00, 555);");
}

TEST_F(OptimizerSQLTests, SimpleSelectTest) {
  catalog::Catalog::GetInstance()->CreateDatabase(DEFAULT_DB_NAME, nullptr);

  CreateAndLoadTable();

  std::vector<StatementResult> result;
  std::vector<FieldInfo> tuple_descriptor;
  std::string error_message;
  int rows_changed;
  std::unique_ptr<optimizer::AbstractOptimizer> optimizer(
      new optimizer::Optimizer());

  std::string query("SELECT * from test");
  auto select_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(select_plan->GetPlanNodeType(), PlanNodeType::SEQSCAN);

  // test small int
  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);
  // Check the return value
  // Should be: 1, 22, 333
  EXPECT_EQ(0, rows_changed);
  EXPECT_EQ(12, result.size());
  EXPECT_EQ("1", TestingSQLUtil::GetResultValueAsString(result, 0));
  EXPECT_EQ("22", TestingSQLUtil::GetResultValueAsString(result, 1));
  EXPECT_EQ("333", TestingSQLUtil::GetResultValueAsString(result, 2));
  EXPECT_EQ("2", TestingSQLUtil::GetResultValueAsString(result, 3));
  EXPECT_EQ("11", TestingSQLUtil::GetResultValueAsString(result, 4));
  EXPECT_EQ("0", TestingSQLUtil::GetResultValueAsString(result, 5));
  EXPECT_EQ("3", TestingSQLUtil::GetResultValueAsString(result, 6));
  EXPECT_EQ("33", TestingSQLUtil::GetResultValueAsString(result, 7));
  EXPECT_EQ("444", TestingSQLUtil::GetResultValueAsString(result, 8));
  EXPECT_EQ("4", TestingSQLUtil::GetResultValueAsString(result, 9));
  EXPECT_EQ("0", TestingSQLUtil::GetResultValueAsString(result, 10));
  EXPECT_EQ("555", TestingSQLUtil::GetResultValueAsString(result, 11));

  // test small int
  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, "SELECT b, a, c from test where a=1", result, tuple_descriptor,
      rows_changed, error_message);
  // Check the return value
  // Should be: 22, 1, 333
  EXPECT_EQ(0, rows_changed);
  EXPECT_EQ("22", TestingSQLUtil::GetResultValueAsString(result, 0));
  EXPECT_EQ("1", TestingSQLUtil::GetResultValueAsString(result, 1));

  // free the database just created
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  catalog::Catalog::GetInstance()->DropDatabaseWithName(DEFAULT_DB_NAME, txn);
  txn_manager.CommitTransaction(txn);
}

TEST_F(OptimizerSQLTests, SelectProjectionTest) {
  catalog::Catalog::GetInstance()->CreateDatabase(DEFAULT_DB_NAME, nullptr);

  CreateAndLoadTable();

  std::vector<StatementResult> result;
  std::vector<FieldInfo> tuple_descriptor;
  std::string error_message;
  int rows_changed;
  std::unique_ptr<optimizer::AbstractOptimizer> optimizer(
      new optimizer::Optimizer());

  std::string query("SELECT a * 5 + b, -1 + c from test");

  // check for plan node type
  auto select_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(select_plan->GetPlanNodeType(), PlanNodeType::PROJECTION);
  EXPECT_EQ(select_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  // test small int
  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);
  // Check the return value
  // Should be: 27, 332
  EXPECT_EQ("27", TestingSQLUtil::GetResultValueAsString(result, 0));
  EXPECT_EQ("332", TestingSQLUtil::GetResultValueAsString(result, 1));

  // free the database just created
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  catalog::Catalog::GetInstance()->DropDatabaseWithName(DEFAULT_DB_NAME, txn);
  txn_manager.CommitTransaction(txn);
}

TEST_F(OptimizerSQLTests, SelectOrderByTest) {
  catalog::Catalog::GetInstance()->CreateDatabase(DEFAULT_DB_NAME, nullptr);

  CreateAndLoadTable();

  std::vector<StatementResult> result;
  std::vector<FieldInfo> tuple_descriptor;
  std::string error_message;
  int rows_changed;
  std::unique_ptr<optimizer::AbstractOptimizer> optimizer(
      new optimizer::Optimizer());

  // Something wrong with column property.
  std::string query("SELECT b from test order by c");

  // check for plan node type
  auto select_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(select_plan->GetPlanNodeType(), PlanNodeType::ORDERBY);
  EXPECT_EQ(select_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  // test order by
  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);
  // Check the return value
  // Should be: 11, 22
  EXPECT_EQ("11", TestingSQLUtil::GetResultValueAsString(result, 0));
  EXPECT_EQ("22", TestingSQLUtil::GetResultValueAsString(result, 1));

  // free the database just created
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  catalog::Catalog::GetInstance()->DropDatabaseWithName(DEFAULT_DB_NAME, txn);
  txn_manager.CommitTransaction(txn);
}

TEST_F(OptimizerSQLTests, SelectLimitTest) {
  catalog::Catalog::GetInstance()->CreateDatabase(DEFAULT_DB_NAME, nullptr);

  CreateAndLoadTable();

  std::vector<StatementResult> result;
  std::vector<FieldInfo> tuple_descriptor;
  std::string error_message;
  int rows_changed;
  std::unique_ptr<optimizer::AbstractOptimizer> optimizer(
      new optimizer::Optimizer());

  std::string query("SELECT b FROM test ORDER BY b LIMIT 2 OFFSET 2");

  // check for plan node type
  auto select_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(select_plan->GetPlanNodeType(), PlanNodeType::LIMIT);
  EXPECT_EQ(select_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::ORDERBY);
  EXPECT_EQ(select_plan->GetChildren()[0]->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);

  EXPECT_EQ(2, result.size());
  EXPECT_EQ("22", TestingSQLUtil::GetResultValueAsString(result, 0));
  EXPECT_EQ("33", TestingSQLUtil::GetResultValueAsString(result, 1));

  // free the database just created
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  catalog::Catalog::GetInstance()->DropDatabaseWithName(DEFAULT_DB_NAME, txn);
  txn_manager.CommitTransaction(txn);
}

TEST_F(OptimizerSQLTests, DeleteSqlTest) {
  catalog::Catalog::GetInstance()->CreateDatabase(DEFAULT_DB_NAME, nullptr);

  CreateAndLoadTable();

  std::vector<StatementResult> result;
  std::vector<FieldInfo> tuple_descriptor;
  std::string error_message;
  int rows_changed;
  std::unique_ptr<optimizer::AbstractOptimizer> optimizer(
      new optimizer::Optimizer());

  // TODO: Test for index scan
  // Delete with predicates
  std::string query("DELETE FROM test WHERE a = 1 and c = 333");

  // check for plan node type
  auto delete_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(delete_plan->GetPlanNodeType(), PlanNodeType::DELETE);
  EXPECT_EQ(delete_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);

  EXPECT_EQ(1, rows_changed);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, "SELECT * FROM test", result, tuple_descriptor, rows_changed, error_message);
  EXPECT_EQ(9, result.size());

  // Delete with predicates
  query = "DELETE FROM test WHERE b = 33";
  optimizer->Reset();
  // check for plan node type
  delete_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(delete_plan->GetPlanNodeType(), PlanNodeType::DELETE);
  EXPECT_EQ(delete_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);

  EXPECT_EQ(1, rows_changed);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, "SELECT * FROM test", result, tuple_descriptor, rows_changed, error_message);
  EXPECT_EQ(6, result.size());


  // Delete with false predicates
  query = "DELETE FROM test WHERE b = 123";
  optimizer->Reset();
  // check for plan node type
  delete_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(delete_plan->GetPlanNodeType(), PlanNodeType::DELETE);
  EXPECT_EQ(delete_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);

  EXPECT_EQ(0, rows_changed);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, "SELECT * FROM test", result, tuple_descriptor, rows_changed, error_message);
  EXPECT_EQ(6, result.size());


  // Full deletion
  query = "DELETE FROM test";
  optimizer->Reset();
  // check for plan node type
  delete_plan =
      TestingSQLUtil::GeneratePlanWithOptimizer(optimizer, query);
  EXPECT_EQ(delete_plan->GetPlanNodeType(), PlanNodeType::DELETE);
  EXPECT_EQ(delete_plan->GetChildren()[0]->GetPlanNodeType(),
            PlanNodeType::SEQSCAN);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, query, result, tuple_descriptor, rows_changed, error_message);

  EXPECT_EQ(2, rows_changed);

  TestingSQLUtil::ExecuteSQLQueryWithOptimizer(
      optimizer, "SELECT * FROM test", result, tuple_descriptor, rows_changed, error_message);
  EXPECT_EQ(0, result.size());

  // free the database just created
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  catalog::Catalog::GetInstance()->DropDatabaseWithName(DEFAULT_DB_NAME, txn);
  txn_manager.CommitTransaction(txn);
}

}  // namespace test
}  // namespace peloton
