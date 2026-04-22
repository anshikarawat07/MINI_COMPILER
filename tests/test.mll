FUNC main() :
    VAR a : INT
    VAR b : INT
    VAR c : INT

    #secure_start
    SET a 2
    PRINT a
    #secure_end

    SET b 20+c
    PRINT b

    #secure_start
    SET c 30
    PRINT c
    #secure_end

    RET 0
END