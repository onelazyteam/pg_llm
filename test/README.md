# pg_llm Test Instructions

This document explains how to run and write tests for the pg_llm plugin.

## Directory Structure

```
test/
├── sql/          # SQL test files
│   ├── 001_basic.sql              # Basic functionality test
│   ├── 002_models.sql            # Model interface test
│   ├── 003_chat.sql              # Conversation function test
│   └── test_report_visualization.sql  # Report and visualization test
└── expected/     # Expected result files
    └── test_report_visualization.out  # Test output results
```

## Running Tests

1. Compile plugin:
```bash
make
```

2. Install plugin:
```bash
make install
```

3. Run tests:
```bash
make installcheck
```

## Test File Description

### 1. Basic Functionality Test (001_basic.sql)
- Test plugin's basic functionality
- Include extension creation, configuration loading, etc.
- Verify basic API availability

### 2. Model Interface Test (002_models.sql)
- Test various LLM model interfaces
- Verify API calls and responses
- Test model configuration and switching

### 3. Conversation Function Test (003_chat.sql)
- Test multi-turn conversation functionality
- Verify context management
- Test conversation history

### 4. Report and Visualization Test (test_report_visualization.sql)
- Test report generation functionality
- Verify chart generation
- Test visualization URL generation

## Writing New Tests

1. Create new test file in `sql` directory, e.g., `004_new_feature.sql`

2. Test file format:
```sql
-- Test preparation
CREATE TABLE test_table (...);

-- Insert test data
INSERT INTO test_table VALUES (...);

-- Test cases
SELECT pg_llm_some_function(...);

-- Clean up test data
DROP TABLE test_table;
```

3. Run new test:
```bash
make installcheck PGOPTIONS="-c search_path=public" REGRESS=004_new_feature
```

## Test Results

- Test results will be saved in `expected` directory
- Each test file corresponds to a `.out` file
- If test fails, a `.diffs` file will be generated showing differences

## Common Issues

1. Test Failure
   - Check database connection
   - Verify plugin installation
   - Check error logs

2. Expected Results Mismatch
   - Check test data correctness
   - Verify SQL query syntax
   - Update expected result files

3. Environment Issues
   - Ensure PostgreSQL version compatibility
   - Check dependency library installation
   - Verify configuration files

## Debugging Tips

1. Use `\set VERBOSITY verbose` to get detailed error messages

2. Add debug output:
```sql
\echo 'Starting test...'
SELECT pg_llm_debug('Test message');
\echo 'Test complete'
```

3. Use temporary tables for testing:
```sql
CREATE TEMP TABLE test_temp AS
SELECT * FROM some_table;
```

## Contributing Tests

1. Fork project
2. Create test branch
3. Add test files
4. Submit Pull Request

## Notes

1. Test Data Cleanup
   - Clean up all test data after completion
   - Use transaction rollback or DROP statements

2. Test Isolation
   - Each test file uses independent test data
   - Avoid dependencies between tests

3. Performance Considerations
   - Control test data volume
   - Avoid time-consuming operations
   - Set reasonable timeout values 