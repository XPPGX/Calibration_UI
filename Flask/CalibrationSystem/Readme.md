# Cali_With_Flask_socketio
## Usage Steps
### Re-compile Cali.c
> Go to "Clib" folder, compile "Cali.c" with
```
$./compile.sh
```
> Go to "Clib/Cali_Code" folder, compile "Auto_Search_Device.c" with
```
$./compile1.sh
```

### Create a virtual environment name "test" 
```
$virtualenv test
```

### Activate the "test" virtual environment
```
$cd {path to test/test/bin}
```
> Where {content} represents the "content" should be modified according to where the "test" folder located.
```
$source activate
```

### Install all the libraries the project needed
> Find requirements.txt, and use following command
```
$pip install -r requirements.txt
```
 
### Running the project
> Find "app.py", run the following command
```
$python app.py
```

### Check the website
Go to browser and use the URL
```
localhost:5000
```

---

## Feature
1. "儀器設定頁面"：
	- 已串接自動偵測周邊設備程序,於自動偵測後顯示機器名稱,類型與序列號.
	- 若有Chroma 51101-8儀器, 會有設定鈕讓user設定每一通道之讀值類型(V, I)與shunt規格.
	- 按下儲存設定後, 可將設定值存於server, 於"校正排程頁面"會用到.
2. "校正排程頁面":
	- 取得腳本檔案列表, 供user選擇腳本
	- 取得周邊設備的資訊, 為每個校正點創建目標儀器選項
	- 可上下拖動校正點, 以調整校正點順序
	- 按下儲存後會將表格內所有資訊都存入server中對應的腳本檔案(腳本檔案中也會存校正點的順序, 目標儀器等資訊)
