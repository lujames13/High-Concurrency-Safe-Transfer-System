# Git 協作指南：基於 Pull Request (PR) 流程

本專案採用 **Feature Branching + 強制 CI/CD 檢查** 的開發模式。`main` 分支是神聖的 (Sacred)，它必須始終處於可編譯、可運行的狀態。所有程式碼變更都必須透過 Pull Request (PR) 才能合併。

---

## I. 標準開發流程 (Standard Workflow)

### Step 1: 確保主線 (main) 為最新並建立分支

在開始任何開發工作之前，請先確保你的本地 `main` 分支是最新的。

```bash
# 1. 切換回 main 分支
git switch main

# 2. 拉取遠端最新程式碼
git pull origin main

# 3. 建立你的功能分支
# 命名規範：feature/<你的任務簡稱>，例如：feature/mq-init, feature/atomic-transfer
git switch -c feature/your-task-name
```

### Step 2: 開發、提交 (Commit) 與推送

在你自己的 Feature Branch 上進行程式碼撰寫。

```bash
# 1. 撰寫你的程式碼 (例如：在 src/common/mq_wrapper.c 實作)

# 2. 編譯與測試 (本地驗證是義務)
# 在根目錄執行：
mkdir build
cd build
cmake ..
make
./bin/test_bank  # 執行你負責模組的單元測試

# 3. 加入變動並提交
git add .
# 提交訊息規範：<類型>: [模組] 描述
# 類型：feat (新功能), fix (修 Bug), docs (文件)
git commit -m "feat: [MQ] Implement logger_mq_init and cleanup"

# 4. 推送到遠端
# 首次推送需設定 upstream
git push -u origin feature/your-task-name
```

### Step 3: 提交 PR 前的關鍵動作：與主線同步 (CRITICAL)

在你發送 PR 之前，必須先將最新的 `main` 程式碼合併到你的分支並解決所有衝突。這是確保整合測試 (Integration Test) 成功的關鍵。

```bash
# 1. 切換回 main 分支
git switch main
git pull origin main  # 確保 main 是最新的

# 2. 切換回你的功能分支
git switch feature/your-task-name

# 3. 將最新的 main 合併進來
git merge main 
# 如果出現 CONFLICT (衝突)，請立即進入 Step 4 解決。
```

### Step 4: 發送 Pull Request (PR)

在 Step 3 完成後，再次 `git push` 上傳你的本地變更和衝突解決結果。打開 GitHub 頁面，系統會提示你建立 PR。

**PR 標題：** 使用與 Commit 相同的規範 (例如：`feat: [Bank Core] 實作 bank_transfer 交易鎖`)

**PR 內容：** 詳細說明你做了哪些修改、測試結果，以及是否有任何需要 Orchestrator (架構師) 審核的介面變動。

**合併條件：**
- 等待 GitHub Actions (CI) 運行狀態檢查
- 只有在 CI 綠燈 (✅) 且至少一位組員 (Code Owner) 核准後，PR 才能被合併

---

## II. 常見錯誤與解決方案 (Troubleshooting)

### 程式碼衝突 (Merge Conflict)

| 錯誤提示 | 解決方案 | 原理解釋 |
|---------|---------|---------|
| `CONFLICT (content): Merge conflict in...` | 1. 手動編輯衝突檔案 (找 `<<<<<<<`, `>>>>>>>` 符號)<br>2. 移除衝突標記並保留正確程式碼<br>3. `git add .`<br>4. `git commit` (保留 Git 預設的 Merge Message) | 衝突必須在本地解決。不要把衝突丟給 PR 讓其他人幫你解 |

### CI 狀態檢查失敗

| 錯誤提示 | 解決方案 | 原理解釋 |
|---------|---------|---------|
| PR 顯示 `build-and-test` 紅叉叉 (❌) | 1. 點擊紅叉叉查看 Log 找出錯誤原因<br>2. 在本地修復程式碼<br>3. `git commit --amend` (修改上一個提交) 或 `git commit -m "fix: CI fail"`<br>4. `git push` | CI 失敗通常是編譯錯誤或 Unit Test 沒過。紅燈不能 Merge，請修復後重新 Push |

### 分支過時 (Out of Date)

| 錯誤提示 | 解決方案 | 原理解釋 |
|---------|---------|---------|
| PR 顯示 `This branch is out-of-date with the base branch` | 執行 Step 3 的同步流程：<br>`git switch main`<br>`git pull origin main`<br>`git switch feature/your-task-name`<br>`git merge main` | 這是因為在你開發期間，其他人的程式碼已經合併到 `main` 了。你需要將這些新變更拉進來，並證明你的程式碼與它們整合後仍然能運行 |

### 無法 Push 到 Main

| 錯誤提示 | 解決方案 | 原理解釋 |
|---------|---------|---------|
| `remote rejected: main -> main (protected branch hook failed)` | 這是預期的結果。`main` 分支被保護。請確認你是在 `feature/` 分支上工作，並發送 Pull Request。除非緊急狀況並獲得 Orchestrator 許可，否則禁止直接 Push 到 `main` | Branch protection 是為了保護 `main` 的穩定性 |

### PR Merge 按鈕被禁用

| 錯誤提示 | 解決方案 | 原理解釋 |
|---------|---------|---------|
| 按鈕為灰色，提示 `Required approval missing` 或 `Status checks failed` | 1. 檢查 CI 是否為綠燈 (✅)<br>2. 聯繫 Orchestrator 或你的 Code Owner 進行 Review 和 Approve | 必須滿足「1 個核准」和「CI 通過」兩個條件才能合併 |

---

## III. 核心原則 (Key Principles)

> **「解決衝突最好的方法，就是避免衝突。」**

務必在發 PR 前，先執行 `git pull origin main`，確保你在本地端處理所有問題。

**Checklist 在發送 PR 前：**

- ✅ 已執行 `git merge main` 並解決所有衝突
- ✅ 本地編譯通過 (`make` 無錯誤)
- ✅ 單元測試通過 (`./bin/test_bank`)
- ✅ Commit 訊息符合規範 (`feat/fix/docs: [模組] 描述`)
- ✅ PR 標題清晰，內容詳細
- ✅ 沒有 Push 到 `main` 分支