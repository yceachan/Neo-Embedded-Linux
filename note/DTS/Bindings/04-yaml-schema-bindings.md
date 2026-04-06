---
title: 编写 JSON-Schema 格式的 Devicetree Bindings
tags: [dts, devicetree, binding, yaml, json-schema]
desc: 详解现代Devicetree使用YAML和JSON-Schema编写Bindings的规范及验证方法
update: 2024-05-24
---

# 编写 JSON-Schema 格式的 Devicetree Bindings

> [!note]
> **Ref:** [Writing Devicetree Bindings in json-schema](../docs/bindings/writing-schema.html)

现代的设备树 (Devicetree) 绑定采用 **JSON-Schema** 词汇表来编写。这些 Schema 文件使用的是 YAML 的 JSON 兼容子集。选择使用 YAML 而不是 JSON，是因为 YAML 对人类更加可读，并且支持添加注释（以 `#` 开头）。

## 1. Schema 结构与内容 (Schema Contents)

每个 Schema 文档都是一个结构化的 JSON-Schema，由一组顶层属性定义。通常，每个文件定义一个 Binding。常用的顶层属性包括：

- **$id**: JSON-Schema 唯一标识符字符串。必须是一个有效的 URI，通常包含 Binding 的文件名和路径。对于 DT Schema，它必须以 `http://devicetree.org/schemas/` 开头。
- **$schema**: 指示该 Schema 文件所遵循的元架构 (meta-schema)，通常为 `http://devicetree.org/meta-schemas/core.yaml#`。
- **title**: 单行文本，简要描述该 Binding 对应的硬件。
- **maintainers**: DT 专有属性。包含该 Binding 维护者的电子邮件地址列表。
- **description**: （可选）多行文本块，包含有关此硬件的详细信息（功能、遵循的标准、数据手册链接等）。YAML 中多行文本有不同的排版标识符：
  - 无标识符：去除换行符和前导空格，双换行代表分段。
  - `>` (Folded)：保留双换行的同时，保留额外缩进行的前导空格和换行。
  - `|` (Literal)：保留所有换行符和空格。
- **select**: （可选）用于匹配要应用此 Schema 的节点。默认情况下，节点根据其 `compatible` 或节点名称进行匹配。大部分 Binding 不需要它。
- **allOf**: （可选）包含其他需要引入的 Schema 列表（如 I2C 或 SPI 控制器的公共 Schema）。
- **properties**: 一组子 Schema，用于定义该 Binding 的所有 DT 属性。
- **patternProperties**: （可选）类似于 `properties`，但属性名使用正则表达式。
- **required**: 一个包含必填 DT 属性（在 `properties` 中定义）的列表。
- **additionalProperties / unevaluatedProperties**: 控制 Schema 如何处理未在 `properties` 或 `patternProperties` 中匹配到的属性。
  - `additionalProperties: false`: 最常见，表示不允许出现未定义的属性。
  - `unevaluatedProperties: false`: 当引用了其他包含任意属性的 Schema 时使用。
  - `additionalProperties: true`: 罕见，通常在被其他 Schema 引用时作为通用总线或部件的 Schema。
- **examples**: （可选）展示如何实现该 Binding 的 DTS 代码片段。**注意：YAML 不允许前导制表符 (Tab)，必须使用空格**。

## 2. 属性的架构设计 (Property Schema)

Schema 的 `properties` 部分包含了该 Binding 的所有属性及其约束条件。

- **通用属性**: 只需要定义额外的约束（如值的个数或允许的具体值）。
- **厂商特定属性 (Vendor-specific)**: 需要更详细的 Schema。除了布尔值外，通常需要引用 `schemas/types.yaml` 中的基础类型，并且**必须**包含 `description` 属性。
- **数组的处理**: JSON-Schema 默认数组大小可变，但在 DT Schema 中通常期望固定大小。这通过 `items` 列表中的元素个数隐式推断。
- **条件约束**: 如果某个 Binding 覆盖多个相似设备且某些属性有所不同：
  - 在顶层 `properties` 中定义最宽泛的约束。
  - 在 `if:then:` 块中进一步收紧特定设备的约束。不要在 `if:then:` 内直接定义新属性。

## 3. 代码风格 (Coding Style)

- **YAML 缩进**: 使用 2 个空格缩进。
- **DTS 示例缩进**: 推荐在 Schema 中的 DTS 示例使用 4 个空格缩进。
- `properties` 和 `required` 内部的条目顺序建议参考 DTS 编码规范以保持一致性。

## 4. 验证与测试 (Testing)

验证 DT Schema 文件和 DTS 文件需要依赖 `dtschema` 工具。

### 4.1 安装依赖

安装 `dtschema` 需要 Python 的包管理工具 `pip` 以及系统开发依赖：

```bash
# Ubuntu / Debian
sudo apt install swig python3-dev
pip3 install dtschema
```

建议同时安装 `yamllint` 用于基本的 YAML 语法检查。

### 4.2 运行检查命令

Linux 内核 Makefile 提供了便捷的检查目标：

- **验证 Binding Schema 自身语法和格式**:
  ```bash
  make dt_binding_check
  ```
  或者只验证单一文件：`make dt_binding_check DT_SCHEMA_FILES=vendor/device.yaml`

- **根据 Schema 验证设备树源文件 (DTS)**:
  ```bash
  make dtbs_check
  ```
  *注意：`dtbs_check` 会跳过任何有错误的 Schema 文件，因此必须先跑 `dt_binding_check` 确保 Schema 无误。*

## 5. 带注释的 Schema 示例

以下是一个简化并带注释的完整 YAML Schema 示例：

```yaml
# SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
%YAML 1.2
---
$id: http://devicetree.org/schemas/example-schema.yaml#
$schema: http://devicetree.org/meta-schemas/core.yaml#

title: 一个示例设备

maintainers:
  - 开发者名字 <developer@example.com>

description: |
  多行文本的详细描述。
  使用 `|` 保留换行和格式。

properties:
  compatible:
    oneOf:
      - items:
          - enum:
              - vendor,soc4-ip
              - vendor,soc3-ip
          - const: vendor,soc1-ip
      - items:
          - const: vendor,soc1-ip

  reg:
    items:
      - description: 核心寄存器 (core registers)
      - description: 辅助寄存器 (aux registers)

  interrupts:
    minItems: 1
    items:
      - description: tx 中断
      - description: rx 中断

  interrupt-controller: true

  # 厂商私有属性必须带有描述并且通常引用基础类型
  vendor,int-property:
    description: 这是一个私有整型属性
    $ref: /schemas/types.yaml#/definitions/uint32
    enum: [2, 4, 6, 8, 10]

required:
  - compatible
  - reg
  - interrupts
  - interrupt-controller

# 当 compatible 为 soc2-ip 时，强制要求具有 foo-supply 属性
allOf:
  - if:
      properties:
        compatible:
          contains:
            const: vendor,soc2-ip
    then:
      required:
        - foo-supply

# 不允许存在 properties 里未定义的其他属性
additionalProperties: false

examples:
  - |
    node@1000 {
        compatible = "vendor,soc4-ip", "vendor,soc1-ip";
        reg = <0x1000 0x80>,
              <0x3000 0x80>;
        interrupts = <10>;
        interrupt-controller;
    };
```
