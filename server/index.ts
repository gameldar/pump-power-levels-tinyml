import express from 'express';
import bodyParser from 'body-parser';
import fs from 'fs';

const app = express();
const port = 8000;

app.use(
    bodyParser.raw({
        // inflate: true,
        type: '*/*'
    })
);

app.post('/adc_samples', (req, res) => {
    // tslint:disable-next-line:no-console
    console.log(`Got ${req.body.length} ADC bytes`);
    // fs.appendFile(`adc-${Date.now()}.raw`, req.body, () => {
    fs.appendFile(`adc.raw`, req.body, () => {
        res.send('OK');
    });
});

app.listen(port, '0.0.0.0', () => {
    // tslint:disable-next-line:no-console
   console.log(`server started at http://0.0.0.0:${port}`);
});
