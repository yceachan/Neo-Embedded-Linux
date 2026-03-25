# Modern imx6ull kernel driver devp using wsl & Agent

## Resource Map

- `docs` :NXP imx6ull芯片平台、100ask开发板厂商等提供的权威先验文档
- `note`:Agent与开发者共同治理的笔记
- `prj`:驱动开发工程base路径
- `sdk`:100ask team 提供的100ask-imx6ull-14*14-evb 板卡的开发环境，符合Linux4.9.8 Kernel的manifests.



## Agent CLI Project Conventions

- **Note yaml header**

  ```markdown
  //always begin markdown with yaml ,but not H1 title.
  ---   
  title:
  tags: [tag1 , ... ] 
  desc: one sentence description
  update: YYYY-MM-dd
  //leave 1 blank new lines before yaml ending line `---`
  ---
  
  //leave 2 blank new lines after yaml block
  # H1 Title 
  ```
  
- **Mermaid** 
  
  - **whenever** naming a node ,especially with chars like `/ \ () （）`,and chinese,using`""`to include the whole node name.
    - e.g. `GpioLib["GPIO库"] -- register --> Sysfs["/sys/class/gpio"]`
  - **SequenceDiagram**:
    - Always include `autonumber` to clearly mark the execution sequence.
    - When using `rect rgb(...)` blocks, the background color must maintain **High Brightness** (each RGB component > 200) to ensure high contrast and readability for black text on light backgrounds.

- 中文知识输出：
  - 最终结论和文档输出必须使用中文，同时保留专业英文术语
  - 高价值的知识输出始终应固化在文件系统而非上下文缓存，当开发者未明确指示路径，Agent应智能判断一个适合的路径，并提出写入建议。
  
- Reference:

  - always respective source of truth , if Agent refer then ,mark it  below the 1st H1 Title :

    - not only web wiki , **but also local code、note、docs。**
    
    - ```md
      # H1 Title
      
      > [!note]
      > **Ref:** [wiki](url)
      
      text...
      ```