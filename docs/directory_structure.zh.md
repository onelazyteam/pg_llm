# 目录结构说明

## 1. 概述

本文档描述了 pg_llm 扩展的目录结构，帮助开发者理解代码组织方式。

## 2. 主要目录

### 2.1 根目录

```
pg_llm/
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 英文说明文档
├── README.zh.md           # 中文说明文档
├── CONTRIBUTING.md        # 英文贡献指南
├── CONTRIBUTING.zh.md     # 中文贡献指南
├── LICENSE                # 许可证文件
└── docs/                  # 文档目录
```

### 2.2 源代码目录

```
src/
├── pg_llm.cpp            # 主扩展文件
├── models/               # 模型相关代码
│   ├── chatgpt_model.cpp
│   ├── deepseek_model.cpp
│   ├── hunyuan_model.cpp
│   ├── qianwen_model.cpp
│   └── llm_interface.h
├── text2sql/            # Text2SQL 功能代码
│   ├── text2sql.cpp
│   └── text2sql.h
└── utils/              # 工具函数
    ├── pg_llm_glog.cpp
    └── pg_llm_glog.h
```

### 2.3 头文件目录

```
include/
├── pg_llm.h            # 公共头文件
├── models/             # 模型头文件
│   └── llm_interface.h
└── text2sql/          # Text2SQL 头文件
    └── text2sql.h
```

### 2.4 SQL 文件目录

```
sql/
├── pg_llm--1.0.sql    # 扩展安装脚本
└── uninstall_pg_llm.sql # 卸载脚本
```

### 2.5 测试目录

```
test/
├── CMakeLists.txt     # 测试构建配置
├── models/            # 模型测试
├── text2sql/         # Text2SQL 测试
└── utils/            # 工具函数测试
```

### 2.6 文档目录

```
docs/
├── ARCHITECTURE.md    # 架构设计文档
├── text2sql_design.md # Text2SQL 设计文档
├── glog_integration.md # GLog 集成文档
└── directory_structure.md # 目录结构文档
```

## 3. 文件说明

### 3.1 核心文件

1. **pg_llm.cpp**
   - 扩展入口点
   - 函数注册
   - 初始化代码

2. **pg_llm.h**
   - 公共接口定义
   - 类型声明
   - 宏定义

### 3.2 模型文件

1. **llm_interface.h**
   - 模型接口定义
   - 通用函数声明
   - 类型定义

2. **模型实现文件**
   - 具体模型实现
   - API 调用封装
   - 错误处理

### 3.3 Text2SQL 文件

1. **text2sql.h**
   - 类定义
   - 接口声明
   - 配置结构

2. **text2sql.cpp**
   - 功能实现
   - 数据库交互
   - 向量处理

### 3.4 工具文件

1. **pg_llm_glog.h**
   - 日志接口
   - 宏定义
   - 配置声明

2. **pg_llm_glog.cpp**
   - 日志实现
   - 初始化代码
   - 工具函数

## 4. 构建系统

### 4.1 CMake 配置

1. **主 CMakeLists.txt**
   - 项目配置
   - 依赖管理
   - 编译选项

2. **测试 CMakeLists.txt**
   - 测试配置
   - 测试依赖
   - 测试目标

### 4.2 构建步骤

1. **配置**
```bash
mkdir build && cd build
cmake ..
```

2. **编译**
```bash
make
```

3. **安装**
```bash
make install
```

## 5. 开发指南

### 5.1 添加新功能

1. **创建新文件**
   - 在适当目录创建
   - 遵循命名规范
   - 添加头文件保护

2. **更新构建系统**
   - 添加新文件到 CMake
   - 配置依赖关系
   - 更新测试配置

### 5.2 代码组织

1. **模块化设计**
   - 功能分离
   - 接口清晰
   - 依赖最小化

2. **文件命名**
   - 使用小写字母
   - 单词间用下划线
   - 保持一致性

## 6. 维护说明

### 6.1 版本控制

1. **分支管理**
   - main: 主分支
   - develop: 开发分支
   - feature/*: 功能分支

2. **提交规范**
   - 清晰的提交信息
   - 相关的修改
   - 完整的测试

### 6.2 文档更新

1. **代码文档**
   - 及时更新注释
   - 保持文档同步
   - 添加使用示例

2. **项目文档**
   - 更新 README
   - 维护变更日志
   - 更新 API 文档 