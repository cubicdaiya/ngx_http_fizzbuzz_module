# nginx_http_fizzbuzz_module

FizzBuzz with nginx

```
location ~ /fizzbuzz$ {
    fizzbuzz $arg_number;
}
```

```
$ curl -l "http://127.0.0.1:80/fizzbuzz?number=72"
<!DOCTYPE html>
<html>
<head>
<title>FizzBuzz with nginx!</title>
</head>
<body>
<p>FizzBuzz(72) = Fizz</p>
</body>
</html>
$
```
