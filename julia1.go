package main

// references
// png https://www.socketloop.com/tutorials/golang-save-image-to-png-jpeg-or-gif-format
// channels http://www.jtolds.com/writing/2016/03/go-channels-are-bad-and-you-should-feel-bad/
// Workers pattern  http://divan.github.io/posts/go_concurrency_visualize/

// compile: go build julia1.go
// run: julia1.exe

import (
		"flag"
		"fmt"
		"image"
		"image/color"
		"image/png"
		"math/rand"
		"sync"
		"os"
		"log"
		"time"
)

// currently a horizontal slice 
var tileSize = 2

// http://stackoverflow.com/questions/27516387/what-is-the-correct-way-to-find-the-min-between-two-integers-in-go
func min(a, b int) int {
    if a < b {
        return a
    }
    return b
}

// julia fractal
func ComputeFractAt(fX, fY float32, max_iter uint) uint {
	var iter uint = 0

	var dist2 float32 = 0.0
	const maxdist2 float32 = float32(2 * 2 * 400);

	// defines which julia fractal we want to compute
//	const float fCx=-0.8f, fCy=0.2f;							// pretty but higher iteration counts might not lead to much slower computations
//	const float fCx=-0.75f, fCy=0.18f;							// pretty but higher iteration counts might not lead to much slower computations
//	const float fCx=-0.73f, fCy=0.176f;							// good for performance measurements
	const fCx, fCy float32 = -0.74543, 0.11301;					// good for performance measurements

	for dist2 <= maxdist2 && iter < max_iter {
		fX2 := fX * fX - fY * fY + fCx
		fY2 := 2 * fX * fY + fCy

		fX = fX2
		fY = fY2

		iter += 1
		dist2 = fX * fX + fY * fY
	}

	return iter
}


// see https://coderwall.com/p/cp5fya/measuring-execution-time-in-go
// Note: this measures until end of function
// example: defer timeTrack(time.Now(), "factorial")
func timeTrack(start time.Time, name string) {
	elapsed := time.Since(start)
	log.Printf("%s took %s", name, elapsed)
}

type juliaContext struct {
	out *image.NRGBA
	dwWidth, dwHeight, dwMinX, dwMinY int
	fMinX, fMinY, fMaxX, fMaxY float32
	max_iter uint
}

func ComputeBlock(in juliaContext) {
	fStepX := (in.fMaxX - in.fMinX) / float32(in.dwWidth)
	fStepY := (in.fMaxY - in.fMinY) / float32(in.dwHeight)

	for dwY := 0; dwY < in.dwHeight; dwY++ {
		fY := in.fMinY + float32(dwY) * fStepY			// precision loss

		for dwX := 0; dwX < in.dwWidth; dwX++ {
			fX := in.fMinX + float32(dwX) * fStepX		// precision loss

			dwInt := ComputeFractAt(fX, fY, in.max_iter)
			grey := uint8(dwInt)

			// line can be commented, it costs very little
			in.out.Set(dwX + in.dwMinX, dwY + in.dwMinY, color.RGBA{grey,grey,grey, 255})
		}
	}
}

func lerp(min, max, alpha float32) float32 {

	return min + (max - min) * alpha;
}




func worker(tasksCh <-chan int, wg *sync.WaitGroup, context juliaContext) {

	defer wg.Done()
	for {
		task, ok := <-tasksCh
		if !ok {
			return
		}

		localContext := context

		div := float32(context.dwHeight)

		localContext.dwHeight = tileSize;
		localContext.dwMinY = context.dwMinY + task * tileSize;

		dwMaxY := min(localContext.dwMinY + tileSize, context.dwMinY + context.dwHeight)

		localContext.fMinY = lerp(context.fMinY, context.fMaxY, float32(localContext.dwMinY) / div);
		localContext.fMaxY = lerp(context.fMinY, context.fMaxY, float32(dwMaxY) / div);

		ComputeBlock(localContext);
	}
}

func pool(wg *sync.WaitGroup, workers, tasks int, context juliaContext) {
	tasksCh := make(chan int)

	for i := 0; i < workers; i++ {
		go worker(tasksCh, wg, context)
	}

	for i := 0; i < tasks; i++ {
		tasksCh <- i
	}

	close(tasksCh)
}


func main() {
	flag.Parse()
	rand.Seed(time.Now().UTC().UnixNano())

	tileFromCpp := 128

	// ~395ms for 1600x900
	imgRect := image.Rect(0, 0, tileFromCpp * 16*4, tileFromCpp * 10*4)
//	imgRect := image.Rect(0, 0, 1600*10, 900*10)
	img := image.NewNRGBA(imgRect)
	//        draw.Draw(img, img.Bounds(), &image.Uniform{color.White}, image.ZP, draw.Src)

	var context juliaContext;

	context.out = img;
	context.dwWidth = imgRect.Max.X - imgRect.Min.X;
	context.dwHeight = imgRect.Max.Y - imgRect.Min.Y;
	context.fMinX, context.fMinY, context.fMaxX, context.fMaxY = -1.6, -1.0, 1.6, 1.0
	context.max_iter = 255

	// zoom out
	context.fMinX *= 1.2
	context.fMinY *= 1.2
	context.fMaxX *= 1.2
	context.fMaxY *= 1.2

	singleTime := 0 * time.Millisecond

	for workercount := 1; workercount < 16; workercount++ {
		start := time.Now()

		if workercount > 1 {
			// multi threaded 128 ms
			var wg sync.WaitGroup
	
			// div and round up
			workcount := (context.dwHeight + tileSize - 1) / tileSize;
		
			wg.Add(workercount)
			go pool(&wg, workercount, workcount, context)
			wg.Wait()
		} else {
			// single threaded 430ms
			ComputeBlock(context);
		}

		elapsed := time.Since(start)

		if workercount == 1 {
			singleTime = elapsed;
		}

		log.Printf("%dx%d i:%d tileSize:%d %d. Time: %s %.2f%%", 
			context.dwWidth, context.dwHeight,context.max_iter,
			tileSize, workercount, elapsed, 
			elapsed.Seconds() / singleTime.Seconds() * 100.0)
	}

	out, err := os.Create("./julia1.png")
	if err != nil {
			fmt.Println(err)
			os.Exit(1)
	}

	err = png.Encode(out, img)

	if err != nil {
			fmt.Println(err)
			os.Exit(1)
	}

	fmt.Println("Generated image to julia1.png \n")
}