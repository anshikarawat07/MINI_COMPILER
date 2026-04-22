FUNC main() :
    # TEST 1: Basic Uninitialized Usage (FAIL)
    VAR a : INT
    VAR b : INT
    SET b a + 5    # Error: 'a' used before initialization

    # TEST 2: Proper Initialization (PASS)
    VAR x : INT
    SET x 10
    PRINT x        # OK

    # TEST 3: Self-Usage (FAIL)
    VAR y : INT
    SET y y + 1    # Error: 'y' used before initialization

    # TEST 4: Nested Scopes (FAIL correctly)
    VAR z : INT
    {
        VAR z : INT
        SET z 100
        PRINT z    # OK (inner z)
    }
    PRINT z        # Error: outer 'z' still uninitialized

    # TEST 5: Conditional/Loop Uncertainty (WARNING)
    VAR w : INT
    IF 1 :
        SET w 50
    END
    PRINT w        # Warning: 'w' possibly uninitialized

    # TEST 6: Chained Dependency (FAIL Propagation)
    VAR m : INT
    VAR n : INT
    VAR p : INT
    SET n m + 1    # Error (m)
    SET p n + 1    # Error (n) because it wasn't initialized due to error in m

    RET 0
END
