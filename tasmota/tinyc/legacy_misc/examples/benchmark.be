# Berry Benchmark — equivalent to TinyC benchmark.tc
# Paste into Tasmota Berry console or save as /benchmark.be
# Measures execution speed of various operations

# Integer arithmetic: 100k iterations, 5 ops each = 500k ops
def bench_int()
    var sum = 0
    var i = 0
    while i < 100000
        sum = sum + i
        sum = sum - (i / 2)
        sum = sum * 3
        sum = sum / 4
        sum = sum % 1000000
        i += 1
    end
    return sum
end

# Float arithmetic: 50k iterations, 5 ops each = 250k ops
def bench_float()
    var sum = 0.0
    var x = 1.0
    var i = 0
    while i < 50000
        sum = sum + x
        x = x * 1.0001
        sum = sum - x * 0.5
        sum = sum * 1.001
        sum = sum / 1.001
        i += 1
    end
    return sum
end

# Array access: 200 fill + 100k random access
def bench_array()
    var arr = list()
    arr.resize(200)
    var i = 0
    while i < 200
        arr[i] = i * 7
        i += 1
    end
    var sum = 0
    i = 0
    while i < 100000
        var idx = (i * 31 + 17) % 200
        sum = sum + arr[idx]
        arr[idx] = sum & 0xFFFF
        i += 1
    end
    return sum
end

# Function call: 100k calls with 3 args
def add3(a, b, c)
    return a + b + c
end

def bench_call()
    var sum = 0
    var i = 0
    while i < 100000
        sum = add3(sum, i, 1)
        sum = sum & 0x7FFFFFFF
        i += 1
    end
    return sum
end

# String operations: 10k iterations
def bench_string()
    var i = 0
    while i < 10000
        var a = 'Hello'
        var b = 'World'
        var c = a + ' ' + b
        var l = size(c)
        i += 1
    end
    return i
end

def run_benchmark()
    print('=== Berry Benchmark ===')
    var t_start = tasmota.millis()
    var t0, t1

    t0 = tasmota.millis()
    bench_int()
    t1 = tasmota.millis()
    var t_int = t1 - t0
    print(format('Integer:  %d ms  (500k ops)', t_int))

    t0 = tasmota.millis()
    bench_float()
    t1 = tasmota.millis()
    var t_float = t1 - t0
    print(format('Float:    %d ms  (250k ops)', t_float))

    t0 = tasmota.millis()
    bench_array()
    t1 = tasmota.millis()
    var t_array = t1 - t0
    print(format('Array:    %d ms  (100k access)', t_array))

    t0 = tasmota.millis()
    bench_call()
    t1 = tasmota.millis()
    var t_call = t1 - t0
    print(format('Calls:    %d ms  (100k calls)', t_call))

    t0 = tasmota.millis()
    bench_string()
    t1 = tasmota.millis()
    var t_string = t1 - t0
    print(format('Strings:  %d ms  (10k iters)', t_string))

    var t_total = tasmota.millis() - t_start
    print(format('-- Total: %d ms --', t_total))
end

run_benchmark()
