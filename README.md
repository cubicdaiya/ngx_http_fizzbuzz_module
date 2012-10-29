# nginx_http_fizzbuzz_module

FizzBuzz with Nginx

```
location /fizzbuzz {
    fizzbuzz $arg_number;
}
```

```
$ curl -l "http://127.0.0.1:80/fizzbuzz?number=72"
<!DOCTYPE html>
<html>
<head>
<title>FizzBuzz with Nginx!</title>
</head>
<body>
<p>FizzBuzz(72) = Fizz</p>
</body>
</html>
$
```
