local sc = require("sc")
local redis = require 'redis'
local client = redis.connect(sc.redis.ip, sc.redis.port)
return client