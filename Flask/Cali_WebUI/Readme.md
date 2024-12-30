# Cali_With_Flask_socketio
## Steps

### Re-compile Cali.c
> Go to "Clib" folder
```
$./compile.sh
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
